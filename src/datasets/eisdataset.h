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
#include <string>
#include <vector>
#include <kisstype/spectra.h>

class EisDataset
{
private:
	virtual eis::Spectra getImpl(size_t index) = 0;

public:
	eis::Spectra get(size_t index);
	virtual size_t size() const = 0;
	virtual size_t classForIndex(size_t index) = 0;
	virtual std::string modelStringForClass(size_t classNum) {return std::string("Unkown");}
	virtual std::string getDescription() {return "";};
	virtual ~EisDataset(){}

	static std::string getOptionsHelp() {return "";}
	static std::vector<std::string> getOptions() {return {};};
	static std::vector<int> getDefaultOptionValues() {return {};};
};
