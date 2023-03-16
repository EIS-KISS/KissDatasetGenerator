#include <set>
#include <filesystem>
#include <eisgenerator/model.h>
#include <eisgenerator/log.h>
#include <eisgenerator/translators.h>
#include <thread>
#include <list>
#include <mutex>
#include <cassert>
#include <memory>

#include "log.h"
#include "options.h"
#include "filterdata.h"
#include "tokenize.h"
#include "randomgen.h"
#include "eisgendata.h"

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

struct Spectrum
{
	Example ex;
	std::string model;
	size_t indexInModel;
};

static bool save(const Spectrum& spectrum, const std::filesystem::path& outDir)
{
	std::string filename("generated");
	filename.push_back('_');
	filename.append(spectrum.model);
	filename.push_back('_');
	filename.append(std::to_string(spectrum.indexInModel));
	filename.append(".csv");
	eis::EisSpectra saveSpectra(spectrum.ex.data, spectrum.model,
								std::to_string(spectrum.ex.label) + ", " + std::to_string(spectrum.indexInModel));
	bool ret = eis::saveToDisk(saveSpectra, outDir/filename);
	if(!ret)
		Log(Log::ERROR)<<"Could not save "<<outDir/filename<<" to disk\n";
	return ret;
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

	EisGeneratorDataset dataset(config.datasetPath, config.desiredSize, 100, 0, true, false);
	Log(Log::INFO)<<"Dataset size: "<<dataset.size();

	std::vector<size_t> classCounts(dataset.classesCount(), 0);
	std::vector<size_t> testCounts(dataset.classesCount(), 0);
	for(size_t i = 0; i < dataset.size(); ++i)
	{
		Spectrum spectrum;
		spectrum.ex = dataset.get(i);
		spectrum.model = dataset.modelStringForClass(spectrum.ex.label);
		spectrum.indexInModel = classCounts[spectrum.ex.label];
		++classCounts[spectrum.ex.label];
		bool ret;
		if(rd::rand(100) < config.testPercent ||
			testCounts[spectrum.ex.label]/static_cast<double>(classCounts[])*100.0 < config.testPercent/2 ||
			testCounts[spectrum.ex.label] == 0)
			ret = save(spectrum, config.outDir/"test");
			++testCounts[spectrum.ex.label]
		else
			ret = save(spectrum, config.outDir/"train");
		if(!ret)
			break;
	}

	return 0;
}
