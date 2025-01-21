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
#include <eisgenerator/model.h>
#include <string>
#include <filesystem>
#include <memory>

#include "eisdataset.h"
#include "spectra.h"

class EisGeneratorDataset :
public EisDataset
{
	struct ModelData
	{
		std::shared_ptr<eis::Model> model;
		size_t sweepSize;
		size_t classNum;

		bool isSingleSweep;
		std::string singleModelParamString;
		std::vector<eis::DataPoint> data;
	};

public:
	static constexpr bool PRINT = false;
	static constexpr size_t DEFAULT_EXAMPLE_COUNT = 1e8;

private:
	std::vector<ModelData> models;
	std::vector<std::string> modelStrings;

	eis::Range omega;
	double noise;

private:
	std::pair<size_t, size_t> getModelAndOffsetForIndex(size_t index) const;
	void addVectorOfModels(const std::vector<std::string>& modelStrs, int64_t desiredSize);

	virtual eis::Spectra getImpl(size_t index) override;

public:
	explicit EisGeneratorDataset(int64_t outputSize = 100, double noiseI = 0);
	explicit EisGeneratorDataset(const std::filesystem::path& path, int64_t desiredSize, int64_t outputSize, double noise);
	explicit EisGeneratorDataset(std::istream& is, int64_t outputSize, int64_t desiredSize, double noise);
	explicit EisGeneratorDataset(const char* cStr, size_t cStrLen, int64_t desiredSize, int64_t outputSize, double noise);

	EisGeneratorDataset(const EisGeneratorDataset& in) = default;
	void addModel(const eis::Model& model);
	void addModel(std::shared_ptr<eis::Model> model);
	EisGeneratorDataset* getTestDataset();
	size_t frequencies();
	void setOmegaRange(eis::Range range);
	size_t singleSweepCount();
	void compileModels();

	virtual size_t classForIndex(size_t index) override;
	virtual std::string modelStringForClass(size_t classNum) override;
	virtual size_t size() const override;
};
