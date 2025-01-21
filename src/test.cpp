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

#include <iostream>
#include <vector>
#include <memory>

int main(int argc, char** argv)
{
	std::vector<std::unique_ptr<int>> ids;
	std::unique_ptr<int> anInt(new int(0));
	ids.push_back(std::move(anInt));
	return 1;
}
