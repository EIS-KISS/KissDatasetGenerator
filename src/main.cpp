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

#include <cstdio>
#include <filesystem>
#include <eisgenerator/model.h>
#include <eisgenerator/log.h>
#include <eisgenerator/translators.h>
#include <kisstype/type.h>
#include <sstream>
#include <string>
#include <thread>
#include <mutex>
#include <cassert>
#include <vector>
#include <set>
#include <fstream>

#include "datasets/eisgendatanoise.h"
#include "log.h"
#include "options.h"
#include "datasets/parameterregressiondataset.h"
#include "datasets/eisdataset.h"
#include "datasets/passfaildataset.h"
#include "datasets/dirloader.h"
#include "datasets/tarloader.h"
#include "randomgen.h"
#include "microtar.h"
#include "hash.h"
#include "tokenize.h"
#include "ploting.h"

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

static std::string constructFilename(const eis::Spectra& spectrum, int offset, const std::string& extension = ".csv")
{
	uint64_t hash = murmurHash64(spectrum.data.data(), spectrum.data.size()*sizeof(*spectrum.data.data()), 8371) + offset;
	std::string model = spectrum.model;
	eis::purgeEisParamBrackets(model);
	std::string filename(model);
	filename.push_back('_');
	filename.append(std::to_string(hash));
	filename.append(extension);
	return filename;
}

static bool save(const eis::Spectra& spectrum, const std::filesystem::path& outDir, std::mutex& saveMutex, std::set<std::string>& filenames, mtar_t* tar, bool saveImages)
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
			if(saveImages)
			{
				std::filesystem::path imagePath = outDir/(tokenize(filename, '.')[0] + ".png");
				auto[real, imag] = eis::eisToValarrays(spectrum.data);

				if(!save2dPlot(imagePath, spectrum.model, "Re(z)", "Im(z)", real, imag))
					Log(Log::WARN)<<"Could not save "<<imagePath;
			}
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
				mtar_t* traintar, mtar_t* testtar, bool eraseLabels, bool noNegative, bool saveImages, std::string overrideModel)
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

		if(!overrideModel.empty())
			spectrum.model = overrideModel;

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
			save(spectrum, outDir/"test", *saveMutex, *filenames, testtar, saveImages);
		else
			save(spectrum, outDir/"train", *saveMutex, *filenames, traintar, saveImages);

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
									  config.testPercent, &printMutex, config.outDir, &saveMutex, &filenames,
									  traintar, testtar, eraseLabels, config.noNegative, config.saveImages, config.overrideModel));
	threads.push_back(std::thread(threadFunc, new Dataset(dataset), i*countPerThread, dataset.size(),
									  config.testPercent, &printMutex, config.outDir, &saveMutex, &filenames,
									  traintar, testtar, eraseLabels, config.noNegative, config.saveImages, config.overrideModel));

	for(std::thread& thread : threads)
		thread.join();
}

std::pair<std::string, int> parseOption(std::string option)
{
	std::vector<std::string> tokens = tokenize(option, '=');
	if(tokens.size() == 1)
		return {option, true};
	else
		return {tokens[0], std::stoi(tokens[1])};
}

template<typename Dataset>
bool parseOptions(std::string stroptions, std::vector<int>& options)
{
	std::vector<std::string> tokens = tokenize(stroptions, ',');
	std::vector<std::pair<std::string, int>> candidates;
	for(std::string token : tokens)
		candidates.push_back(parseOption(token));

	options = Dataset::getDefaultOptionValues();
	std::vector<std::string> optionNames = Dataset::getOptions();

	for(const std::pair<std::string, int>& candidate :candidates)
	{
		auto search = std::find(optionNames.begin(), optionNames.end(), candidate.first);
		if(search == optionNames.end())
		{
			Log(Log::ERROR)<<"Unkown dataset option "<<candidate.first<<"\n\nSupported options for this dataset:\n"<<Dataset::getOptionsHelp();
			return false;
		}

		size_t optionIndex = std::distance(optionNames.begin(), search);
		options[optionIndex] = candidate.second;
	}
	return true;
}

void printDatasetHelp(DatasetMode datasetMode)
{
	Log(Log::INFO)<<"Supported options for this dataset:";
	switch(datasetMode)
	{
		case DATASET_GEN:
			Log(Log::INFO)<<EisGeneratorDataset::getOptionsHelp();
		break;
		case DATASET_PASSFAIL:
			Log(Log::INFO)<<"None";
		break;
		case DATASET_REGRESSION:
			Log(Log::INFO)<<ParameterRegressionDataset::getOptionsHelp();
		break;
		case DATASET_DIR:
			Log(Log::INFO)<<EisDirDataset::getOptionsHelp();
		break;
		case DATASET_TAR:
			Log(Log::INFO)<<TarDataset::getOptionsHelp();
		break;
		default:
			Log(Log::ERROR)<<"Not implemented";
			break;
	}
}

std::string getMetadata(const Config& config, size_t datasetSize, const std::string& role = "unkown")
{
	std::stringstream ss;
	ss<<"{\n";
	ss<<"\t\"DatasetType\" : \""<<datasetModeToStr(config.mode)<<"\",\n";
	ss<<"\t\"DatasetOptions\" : \""<<config.dataOptions<<"\",\n";
	ss<<"\t\"DatasetSize\" : "<<datasetSize<<",\n";
	ss<<"\t\"DatasetRole\" : \""<<role<<"\"\n";
	ss<<"}\n";
	return ss.str();
}

int main(int argc, char** argv)
{
	Log::level = Log::INFO;
	eis::Log::level = eis::Log::ERROR;
	Config config;
	argp_parse(&argp, argc, argv, 0, 0, &config);

	if(config.mode == DATASET_INVALID)
	{
		Log(Log::ERROR)<<"A invalid dataset type was specified";
		return 1;
	}

	if(config.printDatasetHelp)
	{
		printDatasetHelp(config.mode);
		return 0;
	}

	if(config.datasetPath.empty())
	{
		Log(Log::ERROR)<<"A path to a dataset (option -d) must be provided";
		return 1;
	}

	if(config.saveImages && config.tar)
	{
		Log(Log::ERROR)<<"Saveing images to tar is not implemented";
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

	std::vector<int> options;

	Log(Log::INFO)<<"Exporting dataset of type "<<datasetModeToStr(config.mode);

	size_t datasetSize = 0;

	switch(config.mode)
	{
		case DATASET_GEN:
		{
			if(!parseOptions<EisGeneratorDataset>(config.dataOptions, options))
				return 1;
			EisGeneratorDataset dataset(options, config.datasetPath, config.frequencyCount);
			if(!config.range.empty())
				dataset.setOmegaRange(eis::Range::fromString(config.range, config.frequencyCount));
			exportDataset<EisGeneratorDataset>(dataset, config, traintar, testtar);
			datasetSize = dataset.size();
		}
		break;
		case DATASET_PASSFAIL:
		{
			EisGeneratorDataset gendataset(EisGeneratorDataset::getDefaultOptionValues(), config.datasetPath, config.frequencyCount);
			if(!config.range.empty())
				gendataset.setOmegaRange(eis::Range::fromString(config.range, config.frequencyCount));
			PassFaillDataset dataset(&gendataset);
			exportDataset<PassFaillDataset>(dataset, config, traintar, testtar);
			datasetSize = dataset.size();
		}
		break;
		case DATASET_REGRESSION:
		{
			if(!parseOptions<ParameterRegressionDataset>(config.dataOptions, options))
				return 1;
			ParameterRegressionDataset dataset(options, config.datasetPath, config.frequencyCount);
			if(!config.range.empty())
				dataset.setOmegaRange(eis::Range::fromString(config.range, config.frequencyCount));
			exportDataset<ParameterRegressionDataset>(dataset, config, traintar, testtar);
			datasetSize = dataset.size();
		}
		break;
		case DATASET_DIR:
		{
			if(!parseOptions<EisDirDataset>(config.dataOptions, options))
				return 1;
			EisDirDataset dataset(options, config.datasetPath, config.frequencyCount, selectLabelKeys, extraInputKeys);
			size_t removed = dataset.removeLessThan(50);
			Log(Log::INFO)<<"Removed "<<removed<<" spectra as there are not enough examples for this class";
			exportDataset<EisDirDataset>(dataset, config, traintar, testtar);
			datasetSize = dataset.size();
		}
		break;
		case DATASET_TAR:
		{
			if(!parseOptions<TarDataset>(config.dataOptions, options))
				return 1;
			TarDataset dataset(options, config.datasetPath, config.frequencyCount, selectLabelKeys, extraInputKeys);
			exportDataset<TarDataset>(dataset, config, traintar, testtar);
			datasetSize = dataset.size();
		}
		break;
		default:
			Log(Log::ERROR)<<"Not implemented";
			break;
	}

	if(traintar)
	{
		std::string metastr = getMetadata(config, datasetSize, "train");
		mtar_write_file_header(traintar, "meta.json", metastr.size());
		mtar_write_data(traintar, metastr.c_str(), metastr.size());
		mtar_finalize(traintar);
		delete traintar;
	}

	if(testtar)
	{
		std::string metastr = getMetadata(config, datasetSize, "test");
		mtar_write_file_header(testtar, "meta.json", metastr.size());
		mtar_write_data(testtar, metastr.c_str(), metastr.size());
		mtar_finalize(testtar);
		delete testtar;
	}

	if(!config.tar)
	{
		{
			std::string metastr = getMetadata(config, datasetSize, "train");

			std::filesystem::path metaPath = config.outDir/"train"/"meta.json";
			std::ofstream file(metaPath);
			if(!file.is_open())
			{
				Log(Log::ERROR)<<"Could not open "<<metaPath<<" for writeing";
				return 1;
			}
			file<<metastr;
			file.close();
		}

		if(config.testPercent > 0)
		{
			std::string metastr = getMetadata(config, datasetSize, "test");

			std::filesystem::path metaPath = config.outDir/"test"/"meta.json";
			std::ofstream file(metaPath);
			if(!file.is_open())
			{
				Log(Log::ERROR)<<"Could not open "<<metaPath<<" for writeing";
				return 1;
			}
			file<<metastr;
			file.close();
		}
	}

	return 0;
}
