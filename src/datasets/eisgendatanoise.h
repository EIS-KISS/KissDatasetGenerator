/* * KissDatasetGenerator - A generator of datasets for TorchKissAnn
 * Copyright (C) 2025 Carl Klemm <carl@uvos.xyz>
 *
 * This file is part of KissDatasetGenerator.
 *
 * KissDatasetGenerator is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * KissDatasetGenerator is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with KissDatasetGenerator.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <filesystem>
#include <memory>
#include <eisgenerator/model.h>
#include <eisnoise/eisnoise.h>

#include "eisdataset.h"

class EisGeneratorDataset :
public EisDataset
{
	struct ModelData
	{
		std::shared_ptr<eis::Model> model;
		std::vector<size_t> indecies;
		size_t totalCount;
		size_t classNum;
	};

public:
	static constexpr bool PRINT = false;
	static constexpr size_t DEFAULT_EXAMPLE_COUNT = 1e8;

private:
	std::vector<ModelData> models;

	eis::Range omega;
	EisNoise noise;
	bool useEisNoise = true;
	bool normalize = true;
	bool grid = false;
	int desiredSize;
	size_t classCounter = 0;

private:
	std::pair<size_t, size_t> getModelAndOffsetForIndex(size_t index) const;
	void addVectorOfModels(const std::vector<std::string>& modelStrs);

	virtual eis::Spectra getImpl(size_t index) override;
	ModelData* findSameClass(std::string modelStr);

public:
	explicit EisGeneratorDataset(const std::vector<int>& options, int64_t outputSize);
	explicit EisGeneratorDataset(const std::vector<int>& options, const std::filesystem::path& path, int64_t outputSize);
	explicit EisGeneratorDataset(const std::vector<int>& options, std::istream& is, int64_t desiredSize);
	explicit EisGeneratorDataset(const std::vector<int>& options, const char* cStr, size_t cStrLen, int64_t outputSize);

	EisGeneratorDataset(const EisGeneratorDataset& in) = default;
	void addModel(const eis::Model& model, size_t targetExamples);
	void addModel(std::shared_ptr<eis::Model> model, size_t targetExamples);
	EisGeneratorDataset* getTestDataset();
	size_t frequencies();
	void setOmegaRange(eis::Range range);

	static std::string getOptionsHelp();
	static std::vector<std::string> getOptions();
	static std::vector<int> getDefaultOptionValues();
	virtual size_t classForIndex(size_t index) override;
	virtual std::string modelStringForClass(size_t classNum) override;
	virtual size_t size() const override;
};
