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
#include <vector>
#include <string>
#include <filesystem>
#include <kisstype/spectra.h>

#include "eisdataset.h"


class EisDirDataset : public EisDataset
{
private:

	struct FileNameStr
	{
		std::filesystem::path path;
		size_t classNum;
	};

	std::vector<EisDirDataset::FileNameStr> fileNames;
	size_t inputSize;
	std::vector<std::string> modelStrs;
	std::vector<std::string> selectLabels;
	std::vector<std::string> extraInputs;
	bool normalization;

	virtual eis::Spectra getImpl(size_t index) override;

public:
	explicit EisDirDataset(const std::vector<int>& options, const std::string& dirName, int64_t inputSize = 100, std::vector<std::string> selectLabels = {}, std::vector<std::string> extraInputs = {});
	EisDirDataset(const EisDirDataset& in) = default;

	virtual size_t size() const override;

	virtual size_t classForIndex(size_t index) override;
	virtual std::string modelStringForClass(size_t classNum) override;

	static std::string getOptionsHelp();
	static std::vector<std::string> getOptions();
	static std::vector<int> getDefaultOptionValues();
	size_t removeLessThan(size_t examples);
};
