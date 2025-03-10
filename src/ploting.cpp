//
// TorchKissAnn - A collection of tools to train various types of Machine learning
// algorithms on various types of EIS data
// Copyright (C) 2025 Carl Klemm <carl@uvos.xyz>
//
// This file is part of TorchKissAnn.
//
// TorchKissAnn is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// TorchKissAnn is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with TorchKissAnn.  If not, see <http://www.gnu.org/licenses/>.
//

#include "ploting.h"

#ifdef ENABLE_PLOTTING
#include <sciplot/Canvas.hpp>
#include <sciplot/Figure.hpp>
#include <sciplot/Plot2D.hpp>
#include <sciplot/Plot3D.hpp>

bool save2dPlot(const std::filesystem::path& path, const std::string& title, const std::string& xLabel, const std::string& yLabel,
                std::vector<std::pair<std::valarray<float>, std::valarray<float>>> data, bool square, bool log, bool points)
{
		sciplot::Plot2D plot;
	plot.xlabel(xLabel);
	plot.ylabel(yLabel);

	std::valarray<float> xMaxC(data.size());
	std::valarray<float> xMinC(data.size());
	std::valarray<float> yMaxC(data.size());
	std::valarray<float> yMinC(data.size());

	for(size_t i = 0; i < data.size(); ++i)
	{
		xMaxC[i] = data[i].first.max();
		xMinC[i] = data[i].first.min();
		yMaxC[i] = data[i].second.max();
		yMinC[i] = data[i].second.min();
	}

	float xMin = xMinC.min();
	float xMax = xMaxC.max();
	float yMin = yMinC.min();
	float yMax = yMaxC.max();

	if(square)
	{
		xMin = std::min(xMin, yMin);
		yMin = xMin;

		xMax = std::max(xMax, yMax);
		yMax = xMax;
	}

	plot.xrange(xMin, xMax);
	plot.yrange(yMin, yMax);
	if(log)
		plot.ytics().logscale(10);

	for(const std::pair<std::valarray<float>, std::valarray<float>>& curve : data)
	{
		if(points)
			plot.drawPoints(curve.first, curve.second);
		else
			plot.drawCurve(curve.first, curve.second);
	}
	//plot.legend().hide();

	sciplot::Figure fig({{plot}});
	fig.title(title);
	sciplot::Canvas canvas({{fig}});
	canvas.size(640, 480);
	canvas.save(path);
	return true;
}


bool save2dPlot(const std::filesystem::path& path, const std::string& title, const std::string& xLabel, const std::string& yLabel,
	std::valarray<float> xData, std::valarray<float> yData, bool square, bool log, bool points)
{
	return save2dPlot(path, title, xLabel, yLabel, {{xData, yData}}, square, log, points);
}

bool save2dPlot(const std::filesystem::path& path, const std::string& title, const std::string& xLabel, const std::string& yLabel,
	std::vector<float> xData, std::vector<float> yData, bool square, bool log, bool points)
{
	return save2dPlot(path, title, xLabel, yLabel, std::valarray<float>(xData.data(), xData.size()), std::valarray<float>(yData.data(), yData.size()), square, log, points);
}

#else

bool save2dPlot(const std::filesystem::path& path, const std::string& title, const std::string& xLabel, const std::string& yLabel,
	std::valarray<float> xData, std::valarray<float> yData, bool square, bool log, bool points)
{
	(void)path;
	(void)title;
	(void)yLabel;
	(void)xLabel;
	(void)xData;
	(void)yData;
	(void)square;
	(void)log;
	(void)points;
	return true;
}


bool save2dPlot(const std::filesystem::path& path, const std::string& title, const std::string& xLabel, const std::string& yLabel,
	std::vector<float> xData, std::vector<float> yData, bool square, bool log, bool points)
{
	return save2dPlot(path, title, xLabel, yLabel, std::valarray<float>(xData.data(), xData.size()), std::valarray<float>(yData.data(), yData.size()), square, log, points);
}

#endif
