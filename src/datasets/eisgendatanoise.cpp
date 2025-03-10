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

#include "eisgendatanoise.h"

#include <cassert>
#include <math.h>
#include <cstdlib>
#include <eisgenerator/normalize.h>
#include <eisgenerator/basicmath.h>
#include <sstream>
#include <fstream>
#include <algorithm>

#include "spectra.h"
#include "tokenize.h"
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

EisGeneratorDataset::EisGeneratorDataset(const std::vector<int>& options, int64_t outputSize):
omega(10, 1e6, outputSize/2, true)
{
	assert(options.size() == getOptions().size());
	desiredSize = options[0];
	normalize = !options[1];
	useEisNoise = !options[2];
	grid = options[3];
}

EisGeneratorDataset::EisGeneratorDataset(const std::vector<int>& options, std::istream& is, int64_t outputSize):
EisGeneratorDataset(options, outputSize)
{
	std::vector<std::string> circuits = readCircutsFromStream(is);
	if(circuits.empty())
		throw eis::file_error("stream dosent contain any circuits");
	addVectorOfModels(circuits);
}

EisGeneratorDataset::EisGeneratorDataset(const std::vector<int>& options, const std::filesystem::path& path, int64_t outputSize):
EisGeneratorDataset(options, outputSize)
{
	std::ifstream is(path, std::ios::in);
	if(!is.is_open())
		throw eis::file_error("can not open " + path.string());
	std::vector<std::string> circuits = readCircutsFromStream(is);
	if(circuits.empty())
		throw eis::file_error("file dosent contain any circuits: " + path.string());
	addVectorOfModels(circuits);
}

EisGeneratorDataset::EisGeneratorDataset(const std::vector<int>& options, const char* cStr, size_t cStrLen,
										 int64_t outputSize):
EisGeneratorDataset(options, outputSize)
{
	std::stringstream ss(std::string(cStr, cStrLen));
	std::vector<std::string> circuits = readCircutsFromStream(ss);
	if(circuits.empty())
		throw std::runtime_error("array contains no circuits");
	addVectorOfModels(circuits);
}

void EisGeneratorDataset::addVectorOfModels(const std::vector<std::string>& modelStrs)
{
	int64_t sizePerModel = (desiredSize/modelStrs.size())*3;
	if(sizePerModel < 200)
		sizePerModel = 200;

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

EisGeneratorDataset::ModelData* EisGeneratorDataset::findSameClass(std::string modelStr)
{
	for(ModelData& model :models)
	{
		if(model.model->getModelStr() == modelStr)
			return &model;
	}
	return nullptr;
}

void EisGeneratorDataset::addModel(const eis::Model& model, size_t targetSize)
{
	std::shared_ptr<eis::Model> modelPtr(new eis::Model(model));
	addModel(modelPtr, targetSize);
}

void EisGeneratorDataset::addModel(std::shared_ptr<eis::Model> model, size_t targetSize)
{
	ModelData modelData;
	modelData.model = model;

	Log(Log::INFO)<<__func__<<" adding model "<<model->getModelStr();

	size_t steps = model->getRequiredStepsForSweeps();

	if(!grid)
	{
		Log(Log::INFO)<<"Attempting to give "<<targetSize<<" examples";
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
		Log(Log::INFO)<<__func__<<" found "<<modelData.indecies.size()<<" interesting spectra for model "<<model->getModelStr();
	}
	else
	{
		modelData.indecies.clear();
		modelData.indecies.reserve(steps);
		for(size_t i = 0; i < steps; ++i)
			modelData.indecies.push_back(i);
		modelData.totalCount = steps;
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

	models.push_back(modelData);
}

std::pair<size_t, size_t> EisGeneratorDataset::getModelAndOffsetForIndex(size_t index) const
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

eis::Spectra EisGeneratorDataset::getImpl(size_t index)
{
	assert(index < size());

	std::pair<size_t, size_t> modelAndOffset = getModelAndOffsetForIndex(index);
	ModelData& model = models[modelAndOffset.first];
	size_t modelIndex = modelAndOffset.second % model.indecies.size();

	std::vector<eis::DataPoint> data = model.model->executeSweep(omega, model.indecies[modelIndex]);
	assert(data.size());

	if(normalize)
		eis::normalize(data);
	if(useEisNoise)
		noise.add(data);
	eis::noise(data, 0.001, false);

	if(data.size() != omega.count)
	{
		if constexpr(PRINT)
			std::cout<<__func__<<' '<<index<<" rejected as uninteresting\n";
		if(index+1 < size())
			return get(index+1);
		else
			return get(0);
	}

	eis::Spectra spectra(data, model.model->getModelStrWithParam(model.indecies[modelIndex]), typeid(this).name());

	return spectra;
}

size_t EisGeneratorDataset::frequencies()
{
	return omega.count;
}

size_t EisGeneratorDataset::size() const
{
	size_t size = 0;
	for(size_t i = 0; i < models.size(); ++i)
		size += models[i].totalCount;
	return size;
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
	for(ModelData model : models)
	{
		if(model.classNum == classNum)
			return model.model->getModelStr();
	}
	return "invalid";
}

void EisGeneratorDataset::setOmegaRange(eis::Range range)
{
	omega = range;
}

std::string EisGeneratorDataset::getOptionsHelp()
{
	std::stringstream ss;
	ss<<"size=[NUMMBER]:   the size the dataset should have\n";
	ss<<"no-normalization: dont normalize the data\n";
	ss<<"no-noise:         dont use libeisnoise to add noise\n";
	ss<<"grid:             use a parameter grid instead of the eis::model::getRecommendedParamIndices heuristic\n";
	return ss.str();
}

std::vector<std::string> EisGeneratorDataset::getOptions()
{
	return {"size", "no-normalization", "no-noise", "grid"};
}

std::vector<int> EisGeneratorDataset::getDefaultOptionValues()
{
	return{1000, 0, 0, 0};
}
