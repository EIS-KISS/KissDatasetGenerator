#include "eisgendata.h"

#include <assert.h>
#include <math.h>
#include <cstdlib>
#include <eisgenerator/normalize.h>
#include <eisgenerator/basicmath.h>
#include <sstream>
#include <fstream>

#include "log.h"
#include "filterdata.h"
#include "tokenize.h"

using namespace eis;

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

EisGeneratorDataset::EisGeneratorDataset(int64_t outputSize, double noiseI, bool repetitionI):
omega(10, 1e6, outputSize/2, true), noise(noiseI), repetition(repetitionI)
{
}

EisGeneratorDataset::EisGeneratorDataset(std::istream& is, int64_t desiredSize, int64_t outputSize,
										 double noise, bool inductivity, bool repetition):
EisGeneratorDataset(outputSize, noise, repetition)
{
	std::vector<std::string> circuits = readCircutsFromStream(is);
	if(circuits.empty())
		throw eis::file_error("stream dosent contain any circuits");
	addVectorOfModels(circuits, desiredSize, inductivity);
}

EisGeneratorDataset::EisGeneratorDataset(const std::filesystem::path& path, int64_t desiredSize,
										 int64_t outputSize, double noise, bool inductivity, bool repetition):
EisGeneratorDataset(outputSize, noise, repetition)
{
	std::ifstream is(path, std::ios::in);
	if(!is.is_open())
		throw eis::file_error("can not open " + path.string());
	std::vector<std::string> circuits = readCircutsFromStream(is);
	if(circuits.empty())
		throw eis::file_error("file dosent contain any circuits: " + path.string());
	addVectorOfModels(circuits, desiredSize, inductivity);
}

EisGeneratorDataset::EisGeneratorDataset(const char* cStr, size_t cStrLen, int64_t desiredSize,
										 int64_t outputSize, double noise, bool inductivity, bool repetition):
EisGeneratorDataset(outputSize, noise, repetition)
{
	std::stringstream ss(std::string(cStr, cStrLen));
	std::vector<std::string> circuits = readCircutsFromStream(ss);
	if(circuits.empty())
		throw std::runtime_error("array contains no circuits");
	addVectorOfModels(circuits, desiredSize, inductivity);
}

void EisGeneratorDataset::addVectorOfModels(const std::vector<std::string>& modelStrs, int64_t desiredSize, bool inductivity)
{
	int64_t sizePerModel = desiredSize/modelStrs.size();

	assert(sizePerModel > 3);
	for(const std::string& modelStr : modelStrs)
	{
		std::string workModelStr = stripWhitespace(modelStr);
		if(workModelStr.empty())
			continue;

		if(inductivity)
			workModelStr.insert(0, std::string("l{5e-6}-"));

		Log(Log::DEBUG, false)<<__func__<<" adding model "<<workModelStr<<" attempting to give "<<sizePerModel<<" examples ";

		std::shared_ptr<eis::Model> model(new eis::Model(workModelStr));
		if(!model->isReady())
		{
			Log(Log::WARN)<<__func__<<" invalid model string "<<workModelStr;
			continue;
		}
		model->compile();
		if(model->getRequiredStepsForSweeps() > 1)
			model->setParamSweepCountClosestTotal(sizePerModel);
		Log(Log::DEBUG)<<"got "<<model->getRequiredStepsForSweeps();

		addModel(model);
	}

	if(repetition && desiredSize/2 > static_cast<ssize_t>(trueSize()))
	{
		repetitionCount = desiredSize/trueSize();
		Log(Log::WARN)<<__func__<<"Requested dataset size is impossible to fullfill, "
			<<"will use repetition of "<<repetitionCount<<", "
			<<"consider using a expander dataset instead";
	}

	size_t single = singleSweepCount();
	Log(Log::DEBUG)<<__func__<<" dataset now has "<<trueSize()<<" examples from "<<single
		<<" single sweep models and "<<(models.size()-single)<<" regular models and a repetition count of "<<repetitionCount;
}

void EisGeneratorDataset::addModel(const eis::Model& model)
{
	std::shared_ptr<eis::Model> modelPtr(new eis::Model(model));
	addModel(modelPtr);
}

void EisGeneratorDataset::addModel(std::shared_ptr<eis::Model> model)
{
	ModelData modelData;

	size_t steps = model->getRequiredStepsForSweeps();
	const std::string modelStr = model->getModelStr();

	if(steps == 1)
	{
		modelData.isSingleSweep = true;
		modelData.singleModelParamString = model->getModelStrWithParam();
	}
	else
	{
		modelData.model = model;
		modelData.isSingleSweep = false;
	}
	modelData.sweepSize = steps;

	std::vector<std::string>::iterator iterator = std::find(modelStrings.begin(), modelStrings.end(), modelStr);
	size_t index;
	if(iterator == modelStrings.end())
	{
		modelStrings.push_back(modelStr);
		index = modelStrings.size()-1;
	}
	else
	{
		index = std::distance(modelStrings.begin(), iterator);
	}

	modelData.classNum = index;
	models.push_back(modelData);
}

std::pair<size_t, size_t> EisGeneratorDataset::getModelAndOffsetForIndex(size_t index) const
{
	size_t model = 0;
	for(; model < models.size(); ++model)
	{
		if(index < models[model].sweepSize)
			break;
		else
			index -= models[model].sweepSize;
	}

	return std::pair<size_t, size_t>(model, index);
}

eis::EisSpectra EisGeneratorDataset::getImpl(size_t index)
{
	index = index % trueSize();
	std::pair<size_t, size_t> modelAndOffset = getModelAndOffsetForIndex(index);

	std::vector<eis::DataPoint> data;
	if(!models[modelAndOffset.first].isSingleSweep)
	{
		data = models[modelAndOffset.first].model->executeSweep(omega, modelAndOffset.second);
	}
	else
	{
		if(models[modelAndOffset.first].data.size() == 0)
		{
			eis::Model model(models[modelAndOffset.first].singleModelParamString);
			models[modelAndOffset.first].data = model.executeSweep(omega);
		}
		data = models[modelAndOffset.first].data;
	}

	assert(data.size());
	filterData(data, omega.count*2);

	if(data.size() != omega.count)
	{
		if constexpr(PRINT)
			std::cout<<__func__<<' '<<index<<" rejected as uninteresting\n";
		return get(index+1);
	}

	if(noise > 0)
		eis::noise(data, noise, false);

	return eis::EisSpectra(data, "", "", classForIndex(index), classesCount());
}

size_t EisGeneratorDataset::classesCount() const
{
	return modelStrings.size();
}

size_t EisGeneratorDataset::frequencies()
{
	return omega.count;
}

size_t EisGeneratorDataset::size() const
{
	return trueSize()*repetitionCount;
}

size_t EisGeneratorDataset::trueSize() const
{
	size_t size = 0;
	for(size_t i = 0; i < models.size(); ++i)
		size += models[i].sweepSize;
	return size;
}
std::vector<int64_t>  EisGeneratorDataset::classCounts()
{
	std::vector<int64_t> out(classesCount(), 0);
	for(size_t i = 0; i < models.size(); ++i)
		out[models[i].classNum] = out[models[i].classNum] + static_cast<int64_t>(models[i].sweepSize);
	return out;
}

size_t EisGeneratorDataset::classForIndex(size_t index)
{
	std::pair<size_t, size_t> modelAndOffset = getModelAndOffsetForIndex(index);
	return models[modelAndOffset.first].classNum;
}

EisGeneratorDataset* EisGeneratorDataset::getTestDataset()
{
	return this;
}

std::string EisGeneratorDataset::modelStringForClass(size_t classNum)
{
	if(classNum >= modelStrings.size())
		return "invalid";
	else
		return modelStrings[classNum];
}

void EisGeneratorDataset::setOmegaRange(eis::Range range)
{
	omega = range;
	for(ModelData& model : models)
	{
		if(model.isSingleSweep)
			model.data.clear();
	}
}

size_t EisGeneratorDataset::singleSweepCount()
{
	size_t count = 0;
	for(ModelData& model : models)
	{
		if(model.isSingleSweep)
			++count;
	}
	return count;
}
