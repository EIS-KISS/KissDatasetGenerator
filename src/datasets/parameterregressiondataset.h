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

#include <kisstype/type.h>
#include <eisgenerator/model.h>
#include <string>

#include "eisdataset.h"

class ParameterRegressionDataset: public EisDataset
{
public:
	static constexpr size_t DEFAULT_EXAMPLE_COUNT = 1e8;

private:
	eis::Model model;

	eis::Range omega;
	size_t sweepCount;
	size_t parameterCount;
	bool drt;

private:
	virtual eis::Spectra getImpl(size_t index) override;
	static fvalue max(const std::vector<eis::DataPoint>& data);

public:
	explicit ParameterRegressionDataset(const std::vector<int>& options, const std::string& model, int64_t outputSize = 100);
	ParameterRegressionDataset(const ParameterRegressionDataset& in) = default;

	void setOmegaRange(eis::Range range);

	virtual size_t classForIndex(size_t index) override;
	virtual std::string modelStringForClass(size_t classNum) override;
	virtual size_t size() const override;

	static std::string getOptionsHelp();
	static std::vector<std::string> getOptions();
	static std::vector<int> getDefaultOptionValues();
};
