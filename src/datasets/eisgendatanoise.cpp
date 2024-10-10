#include "eisgendatanoise.h"

#include <cassert>
#include <math.h>
#include <cstdlib>
#include <eisgenerator/normalize.h>
#include <eisgenerator/basicmath.h>
#include <sstream>
#include <fstream>
#include <algorithm>

#include "log.h"
#include "filterdata.h"
#include "spectra.h"
#include "tokenize.h"
#include "filterdata.h"
#include "../log.h"

static std::vector<std::string> readCircutsFromStream(std::istream& ss)
{
	std::vector<std::string> out;

	while(ss.good())
	{
		std::string line;
		std::getline(ss, line);
		if(!line.empty() && line[0] != '#')
			out.push_back(line);
	}

	return out;
}

EisGeneratorDatasetNoise::EisGeneratorDatasetNoise(int64_t outputSize):
omega(10, 1e6, outputSize/2, true)
{
}

EisGeneratorDatasetNoise::EisGeneratorDatasetNoise(std::istream& is, int64_t desiredSize, int64_t outputSize):
EisGeneratorDatasetNoise(outputSize)
{
	std::vector<std::string> circuits = readCircutsFromStream(is);
	if(circuits.empty())
		throw eis::file_error("stream dosent contain any circuits");
	addVectorOfModels(circuits, desiredSize);
}

EisGeneratorDatasetNoise::EisGeneratorDatasetNoise(const std::filesystem::path& path, int64_t desiredSize, int64_t outputSize):
EisGeneratorDatasetNoise(outputSize)
{
	std::ifstream is(path, std::ios::in);
	if(!is.is_open())
		throw eis::file_error("can not open " + path.string());
	std::vector<std::string> circuits = readCircutsFromStream(is);
	if(circuits.empty())
		throw eis::file_error("file dosent contain any circuits: " + path.string());
	addVectorOfModels(circuits, desiredSize);
}

EisGeneratorDatasetNoise::EisGeneratorDatasetNoise(const char* cStr, size_t cStrLen, int64_t desiredSize,
										 int64_t outputSize):
EisGeneratorDatasetNoise(outputSize)
{
	std::stringstream ss(std::string(cStr, cStrLen));
	std::vector<std::string> circuits = readCircutsFromStream(ss);
	if(circuits.empty())
		throw std::runtime_error("array contains no circuits");
	addVectorOfModels(circuits, desiredSize);
}

void EisGeneratorDatasetNoise::addVectorOfModels(const std::vector<std::string>& modelStrs, int64_t desiredSize)
{
	int64_t sizePerModel = (desiredSize/modelStrs.size())*3;

	assert(sizePerModel > 3);
	for(const std::string& modelStr : modelStrs)
	{
		std::string workModelStr = stripWhitespace(modelStr);
		if(workModelStr.empty())
			continue;

		std::shared_ptr<eis::Model> model(new eis::Model(workModelStr));
		if(model->getRequiredStepsForSweeps() > 1)
			model->setParamSweepCountClosestTotal(sizePerModel);
		addModel(model, sizePerModel);
	}
	Log(Log::INFO)<<__func__<<" dataset now has "<<size()<<" examples from "<<models.size()<<" models";
}

EisGeneratorDatasetNoise::ModelData* EisGeneratorDatasetNoise::findSameClass(std::string modelStr)
{
	for(ModelData& model :models)
	{
		if(model.model->getModelStr() == modelStr)
			return &model;
	}
	return nullptr;
}

void EisGeneratorDatasetNoise::addModel(const eis::Model& model, size_t targetSize)
{
	std::shared_ptr<eis::Model> modelPtr(new eis::Model(model));
	addModel(modelPtr, targetSize);
}

void EisGeneratorDatasetNoise::addModel(std::shared_ptr<eis::Model> model, size_t targetSize)
{
	ModelData modelData;
	modelData.model = model;

	Log(Log::INFO)<<__func__<<" adding model "<<model->getModelStr()<<" attempting to give "<<targetSize<<" examples";

	size_t steps = model->getRequiredStepsForSweeps();
	const std::string modelStr = model->getModelStr();

	if(steps == 1)
	{
		modelData.indecies = {0};
	}
	else
	{
		model->compile();
		modelData.indecies = model->getRecommendedParamIndices(omega, 0.01, true);
	}
	modelData.totalCount = targetSize;

	if(modelData.indecies.empty())
	{
		modelData.indecies = {0};
		modelData.totalCount = std::min(static_cast<size_t>(1000), targetSize);
	}

	ModelData* candidate = findSameClass(model->getModelStr());
	if(candidate)
	{
		modelData.classNum = candidate->classNum;
	}
	else
	{
		modelData.classNum = classCounter;
		++classCounter;
	}

	Log(Log::INFO)<<__func__<<" found "<<modelData.indecies.size()<<" interesting spectra for model "<<model->getModelStr();

	models.push_back(modelData);
}

std::pair<size_t, size_t> EisGeneratorDatasetNoise::getModelAndOffsetForIndex(size_t index) const
{
	size_t model = 0;
	for(; model < models.size(); ++model)
	{
		if(index < models[model].totalCount)
			break;
		else
			index -= models[model].totalCount;
	}

	return std::pair<size_t, size_t>(model, index);
}

eis::Spectra EisGeneratorDatasetNoise::getImpl(size_t index)
{
	assert(index < size());

	std::pair<size_t, size_t> modelAndOffset = getModelAndOffsetForIndex(index);
	ModelData& model = models[modelAndOffset.first];
	size_t modelIndex = modelAndOffset.second % model.indecies.size();

	std::vector<eis::DataPoint> data = model.model->executeSweep(omega, model.indecies[modelIndex]);
	assert(data.size());

	eis::normalize(data);
	noise.add(data);
	eis::noise(data, 0.001, false);
	filterData(data, omega.count*2);

	if(data.size() != omega.count)
	{
		if constexpr(PRINT)
			std::cout<<__func__<<' '<<index<<" rejected as uninteresting\n";
		if(index+1 < size())
			return get(index+1);
		else
			return get(0);
	}

	eis::Spectra spectra(data, model.model->getModelStr(), typeid(this).name());

	return spectra;
}

size_t EisGeneratorDatasetNoise::frequencies()
{
	return omega.count;
}

size_t EisGeneratorDatasetNoise::size() const
{
	size_t size = 0;
	for(size_t i = 0; i < models.size(); ++i)
		size += models[i].totalCount;
	return size;
}

size_t EisGeneratorDatasetNoise::classForIndex(size_t index)
{
	std::pair<size_t, size_t> modelAndOffset = getModelAndOffsetForIndex(index);
	return models[modelAndOffset.first].classNum;
}

EisGeneratorDatasetNoise* EisGeneratorDatasetNoise::getTestDataset()
{
	return this;
}

std::string EisGeneratorDatasetNoise::modelStringForClass(size_t classNum)
{
	for(ModelData model : models)
	{
		if(model.classNum == classNum)
			return model.model->getModelStr();
	}
	return "invalid";
}

void EisGeneratorDatasetNoise::setOmegaRange(eis::Range range)
{
	omega = range;
}

