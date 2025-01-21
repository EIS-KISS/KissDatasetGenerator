//
// KissDatasetGenerator - A generator of datasets for TorchKissAnn
// Copyright (C) 2025 Carl Klemm <carl@uvos.xyz>
//
// This file is part of KissDatasetGenerator.
//
// KissDatasetGenerator is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// KissDatasetGenerator is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with KissDatasetGenerator.  If not, see <http://www.gnu.org/licenses/>.
//

#include <filesystem>
#include <eisgenerator/model.h>
#include <eisgenerator/log.h>
#include <eisgenerator/translators.h>
#include <kisstype/type.h>
#include <string>
#include <thread>
#include <mutex>
#include <cassert>
#include <vector>
#include <set>

#include "datasets/eisgendatanoise.h"
#include "log.h"
#include "options.h"
#include "datasets/parameterregressiondataset.h"
#include "datasets/eisdataset.h"
#include "datasets/eisgendata.h"
#include "datasets/passfaildataset.h"
#include "datasets/dirloader.h"
#include "datasets/tarloader.h"
#include "randomgen.h"
#include "microtar.h"
#include "hash.h"
#include "tokenize.h"

static bool checkDir(const std::filesystem::path& outDir)
{
	if(!std::filesystem::is_directory(outDir))
	{
		if(!std::filesystem::create_directory(outDir))
		{
			std::cerr<<outDir<<" dose not exist and can not be created\n";
			return false;
		}
	}
	return true;
}

static std::string constructFilename(const eis::Spectra& spectrum, int offset)
{
	uint64_t hash = murmurHash64(spectrum.data.data(), spectrum.data.size()*sizeof(*spectrum.data.data()), 8371) + offset;
	std::string model = spectrum.model;
	eis::purgeEisParamBrackets(model);
	std::string filename(model);
	filename.push_back('_');
	filename.append(std::to_string(hash));
	filename.append(".csv");
	return filename;
}

static bool save(const eis::Spectra& spectrum, const std::filesystem::path& outDir, std::mutex& saveMutex, std::set<std::string>& filenames, mtar_t* tar = nullptr)
{
	bool ret = true;
	std::string filename = constructFilename(spectrum, 0);

	{
		std::scoped_lock<std::mutex> lk(saveMutex);
		if(filenames.count(filename) == 1)
		{
			Log(Log::WARN)<<"Dataset contains several spectra with the same hash at "<<filename;
			for(int i = 1;; ++i)
			{
				filename = constructFilename(spectrum, i);
				if(filenames.count(filename) == 0)
					break;
			}
		}
		filenames.insert(filename);
	}

	if(!tar)
	{
		try
		{
			spectrum.saveToDisk(outDir/filename);
		}
		catch(eis::file_error &err)
		{
			Log(Log::ERROR)<<"Could not save "<<outDir/filename<<" to disk\n";
		}
	}
	else
	{
		std::stringstream ss;
		spectrum.saveToStream(ss);

		std::scoped_lock<std::mutex> lk(saveMutex);
		mtar_write_file_header(tar, filename.c_str(), ss.str().size());
		mtar_write_data(tar, ss.str().c_str(), ss.str().size());
	}

	return ret;
}

void threadFunc(EisDataset* dataset, size_t begin, size_t end, int testPercent, std::mutex* printMutex,
				const std::filesystem::path outDir, std::mutex* saveMutex, std::set<std::string>* filenames,
				mtar_t* traintar, mtar_t* testtar, bool eraseLabels, bool noNegative)
{
	printMutex->lock();
	Log(Log::INFO)<<"Thread doing "<<begin<<" to "<<end-1;
	printMutex->unlock();
	int loggedFor = 0;
	size_t dataSize = 0;
	for(size_t i = begin; i < end; ++i)
	{
		eis::Spectra spectrum = dataset->get(i);
		if(spectrum.data.empty())
		{
			std::scoped_lock lock(*printMutex);
			Log(Log::WARN)<<"Skipping datapoint "<<i;
			continue;
		}

		if(eraseLabels)
		{
			spectrum.setLabels(std::vector<float>());
			spectrum.labelNames = std::vector<std::string>();
		}
		else if(noNegative)
		{
			bool skip = false;
			for(double label : spectrum.labels)
			{
				if(label < 0.0)
				{
					skip = true;
					break;
				}
			}
			if(skip)
				continue;
		}

		if(dataSize == 0)
		{
			dataSize = spectrum.data.size();
		}
		else if(dataSize != spectrum.data.size())
		{
			std::scoped_lock lock(*printMutex);
			Log(Log::WARN)<<"Data at index "<<i<<" has size "<<spectrum.data.size()<<" but "<<dataSize<<" was expected!!";
		}

		bool test = (testPercent > 0 && rd::rand(100) < testPercent);

		if(test)
			save(spectrum, outDir/"test", *saveMutex, *filenames, testtar);
		else
			save(spectrum, outDir/"train", *saveMutex, *filenames, traintar);

		int percent = ((i-begin)*100)/(end-begin);
		if(percent != loggedFor)
		{
			loggedFor = percent;
			std::scoped_lock lock(*printMutex);
			Log(Log::INFO)<<begin<<" -> "<<end<<' '<<percent<<'%';
		}
	}
	delete dataset;
}

template <typename Dataset>
void exportDataset(Dataset& dataset, const Config& config, mtar_t* traintar, mtar_t* testtar)
{
	Log(Log::INFO)<<"Dataset size: "<<dataset.size()<<" "<<dataset.modelStringForClass(0);

	std::mutex saveMutex;
	std::mutex printMutex;
	std::set<std::string> filenames;

	std::vector<std::thread> threads;
	size_t threadCount = std::thread::hardware_concurrency()*1.5;
	size_t countPerThread = dataset.size()/threadCount;
	size_t i = 0;

	bool eraseLabels = config.selectLabels.empty() && config.selectLabelsSet;

	Log(Log::INFO)<<"Spawing "<<threadCount<<" treads";
	for(; i < threadCount-1; ++i)
		threads.push_back(std::thread(threadFunc, new Dataset(dataset), i*countPerThread, (i+1)*countPerThread,
									  config.testPercent, &printMutex, config.outDir, &saveMutex, &filenames, traintar, testtar, eraseLabels, config.noNegative));
	threads.push_back(std::thread(threadFunc, new Dataset(dataset), i*countPerThread, dataset.size(),
									  config.testPercent, &printMutex, config.outDir, &saveMutex, &filenames, traintar, testtar, eraseLabels, config.noNegative));

	for(std::thread& thread : threads)
		thread.join();
}

int main(int argc, char** argv)
{
	Log::level = Log::INFO;
	eis::Log::level = eis::Log::ERROR;
	Config config;
	argp_parse(&argp, argc, argv, 0, 0, &config);

	if(config.datasetPath.empty())
	{
		Log(Log::ERROR)<<"A path to a dataset (option -d) must be provided";
		return 1;
	}

	if(config.mode == DATASET_INVALID)
	{
		Log(Log::ERROR)<<"A invalid dataset type was specified";
		return 1;
	}

	std::vector<std::string> selectLabelKeys = config.selectLabels.empty() ? std::vector<std::string>() : tokenize(config.selectLabels, ',');
	std::vector<std::string> extraInputKeys = config.extaInputs.empty() ? std::vector<std::string>() : tokenize(config.extaInputs, ',');

	mtar_t* traintar = nullptr;
	mtar_t* testtar = nullptr;
	if(!config.tar)
	{
		bool ret = checkDir(config.outDir);
		if(!ret)
			return 3;
		ret = checkDir(config.outDir/"train");
		if(!ret)
			return 3;
		if(config.testPercent > 0)
		{
			ret = checkDir(config.outDir/"test");
			if(!ret)
				return 3;
		}
	}
	else
	{
		if(config.tar)
		{
			traintar = new mtar_t;
			int ret = mtar_open(traintar, (config.outDir.string().append("_train.tar")).c_str(), "w");
			if(ret != 0)
			{
				Log(Log::ERROR)<<"Could not create tar archive at "<<config.outDir.c_str();
				delete traintar;
				return 3;
			}
			if(config.testPercent > 0)
			{
				testtar = new mtar_t;
				ret = mtar_open(testtar, (config.outDir.string() + "_test.tar").c_str(), "w");
				if(ret != 0)
				{
					Log(Log::ERROR)<<"Could not create tar archive at "<<config.outDir.c_str();
					delete traintar;
					delete testtar;
					return 4;
				}
			}
			config.outDir = "";
		}
	}

	Log(Log::INFO)<<"Exporting dataset of type "<<datasetModeToStr(config.mode);

	switch(config.mode)
	{
		case DATASET_GEN:
		{
			EisGeneratorDataset dataset(config.datasetPath, config.desiredSize, config.frequencyCount, 0);
			if(!config.range.empty())
				dataset.setOmegaRange(eis::Range::fromString(config.range, config.frequencyCount));
			exportDataset<EisGeneratorDataset>(dataset, config, traintar, testtar);
		}
		break;
		case DATASET_GEN_NOISE:
		{
			EisGeneratorDatasetNoise dataset(config.datasetPath, config.desiredSize, config.frequencyCount);
			if(!config.range.empty())
				dataset.setOmegaRange(eis::Range::fromString(config.range, config.frequencyCount));
			exportDataset<EisGeneratorDatasetNoise>(dataset, config, traintar, testtar);
		}
		break;
		case DATASET_PASSFAIL:
		{
			EisGeneratorDataset gendataset(config.datasetPath, config.desiredSize, config.frequencyCount, 0);
			if(!config.range.empty())
				gendataset.setOmegaRange(eis::Range::fromString(config.range, config.frequencyCount));
			PassFaillDataset dataset(&gendataset);
			exportDataset<PassFaillDataset>(dataset, config, traintar, testtar);
		}
		break;
		case DATASET_REGRESSION:
		{
			ParameterRegressionDataset dataset(config.datasetPath, config.desiredSize, config.frequencyCount, 0);
			if(!config.range.empty())
				dataset.setOmegaRange(eis::Range::fromString(config.range, config.frequencyCount));
			exportDataset<ParameterRegressionDataset>(dataset, config, traintar, testtar);
		}
		break;
		case DATASET_DIR:
		{
			EisDirDataset dataset(config.datasetPath, config.frequencyCount, selectLabelKeys, extraInputKeys, config.normalization);
			size_t removed = dataset.removeLessThan(50);
			Log(Log::INFO)<<"Removed "<<removed<<" spectra as there are not enough examples for this class";
			exportDataset<EisDirDataset>(dataset, config, traintar, testtar);
		}
		break;
		case DATASET_TAR:
		{
			TarDataset dataset(config.datasetPath, config.frequencyCount, selectLabelKeys, extraInputKeys, config.normalization);
			exportDataset<TarDataset>(dataset, config, traintar, testtar);
		}
		break;
		default:
			Log(Log::ERROR)<<"Not implemented";
			break;
	}

	if(traintar)
	{
		mtar_finalize(traintar);
		delete traintar;
	}

	if(testtar)
	{
		mtar_finalize(testtar);
		delete testtar;
	}

	return 0;
}
