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

static bool save(const eis::Spectra& spectrum, const std::filesystem::path& outDir, std::mutex& saveMutex, mtar_t* tar = nullptr)
{
	uint64_t hash = murmurHash64(spectrum.data.data(), spectrum.data.size()*sizeof(*spectrum.data.data()), 8371);
	std::string model = spectrum.model;
	eis::purgeEisParamBrackets(model);
	std::string filename(model);
	filename.push_back('_');
	filename.append(std::to_string(hash));
	filename.append(".csv");

	bool ret = true;

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
				const std::filesystem::path outDir, std::mutex* saveMutex, mtar_t* traintar, mtar_t* testtar)
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
			save(spectrum, outDir/"test", *saveMutex, testtar);
		else
			save(spectrum, outDir/"train", *saveMutex, traintar);

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

	std::vector<std::thread> threads;
	size_t threadCount = std::thread::hardware_concurrency()*1.5;
	size_t countPerThread = dataset.size()/threadCount;
	size_t i = 0;

	Log(Log::INFO)<<"Spawing "<<threadCount<<" treads";
	for(; i < threadCount-1; ++i)
		threads.push_back(std::thread(threadFunc, new Dataset(dataset), i*countPerThread, (i+1)*countPerThread,
									  config.testPercent, &printMutex, config.outDir, &saveMutex, traintar, testtar));
	threads.push_back(std::thread(threadFunc, new Dataset(dataset), i*countPerThread, dataset.size(),
									  config.testPercent, &printMutex, config.outDir, &saveMutex, traintar, testtar));

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
	std::vector<std::string> extraInputKeys = config.extaInputs.empty() ? std::vector<std::string>() : tokenize(config.extaInputs, ',');;

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
			EisGeneratorDataset dataset(config.datasetPath, config.desiredSize, 100, 0);
			exportDataset<EisGeneratorDataset>(dataset, config, traintar, testtar);
		}
		break;
		case DATASET_GEN_NOISE:
		{
			EisGeneratorDatasetNoise dataset(config.datasetPath, config.desiredSize, 100);
			exportDataset<EisGeneratorDatasetNoise>(dataset, config, traintar, testtar);
		}
		break;
		case DATASET_PASSFAIL:
		{
			EisGeneratorDataset gendataset(config.datasetPath, config.desiredSize, 100, 0);
			PassFaillDataset dataset(&gendataset);
			exportDataset<PassFaillDataset>(dataset, config, traintar, testtar);
		}
		break;
		case DATASET_REGRESSION:
		{
			ParameterRegressionDataset dataset(config.datasetPath, config.desiredSize, 100, 0);
			exportDataset<ParameterRegressionDataset>(dataset, config, traintar, testtar);
		}
		break;
		case DATASET_DIR:
		{
			EisDirDataset dataset(config.datasetPath, 100, selectLabelKeys, extraInputKeys, config.normalization);
			exportDataset<EisDirDataset>(dataset, config, traintar, testtar);
		}
		break;
		case DATASET_TAR:
		{
			TarDataset dataset(config.datasetPath, 100, selectLabelKeys, extraInputKeys, config.normalization);
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
