#include <algorithm>
#include <eisdrt/eisdrt.h>
#include <eisdrt/types.h>
#include <complex>
#include <kisstype/type.h>
#include <eisgenerator/basicmath.h>
#include <limits>

#include "parameterregressiondataset.h"
#include "../log.h"

ParameterRegressionDataset::ParameterRegressionDataset(const std::string& modelStr, int64_t desiredSize, int64_t outputSize, double noiseI, bool drtI):
model(modelStr), omega(1, 10e6, drtI ? outputSize : outputSize/2, true), noise(noiseI), drt(drtI)
{
	model.compile();
	sweepCount = model.getRequiredStepsForSweeps();
	model.setParamSweepCountClosestTotal(desiredSize);
	sweepCount = model.getRequiredStepsForSweeps();
	parameterCount = model.getParameterCount();
}

eis::Spectra ParameterRegressionDataset::getImpl(size_t index)
{
	std::vector<eis::DataPoint> data = model.executeSweep(omega, index);

	assert(data.size());
	if(drt)
	{
		FitMetrics fm;
		try {
			fvalue rSeries;
			std::vector<fvalue> drt = calcDrt(data, fm, FitParameters(1000), &rSeries);
			std::vector<fvalue> omegas = omega.getRangeVector();
			assert(drt.size() == omegas.size());

			if(*drt.begin() > 0.001)
			{
				Log(Log::INFO)<<"Drt low side incompleate";
				return eis::Spectra();
			}

			if(drt.back() > 0.001)
			{
				Log(Log::INFO)<<"Drt high side incompleate";
				return eis::Spectra();
			}

			if(*std::max_element(drt.begin(), drt.end()) < 0.001)
			{
				Log(Log::INFO)<<"Drt is empty, discarding";
				return eis::Spectra();
			}

			std::vector<eis::DataPoint> recalculatedSpectra = calcImpedance(drt, rSeries, omegas);
			fvalue dist = eis::eisNyquistDistance(data, recalculatedSpectra);
			if(dist > 2)
			{
				Log(Log::DEBUG)<<"Drt is of poor quality, discarding";
				return eis::Spectra();
			}

			data.assign(drt.size(), eis::DataPoint());
			for(size_t i = 0; i < drt.size(); ++i)
			{
				data[i].im = std::complex<fvalue>(drt[i], 0);
				data[i].omega = omegas[i];
			}
		}
		catch (const drt_error& ex)
		{
			Log(Log::DEBUG)<<"Drt calculation failed!";
			index++;
			if(index >= size())
				index = 0;
			return eis::Spectra();
		}
	}

	eis::Spectra spectra(data, model.getModelStrWithParam(), typeid(this).name());

	spectra.labelNames = model.getParameterNames();
	spectra.setLabels(model.getFlatParameters());
	return spectra;
}

size_t ParameterRegressionDataset::size() const
{
	return sweepCount;
}

size_t ParameterRegressionDataset::classForIndex(size_t index)
{
	(void)index;
	return 0;
}

std::string ParameterRegressionDataset::modelStringForClass(size_t classNum)
{
	std::vector<eis::Componant*> componants = model.getFlatComponants();
	for(eis::Componant* componant : componants)
	{
		if(classNum > componant->paramCount())
		{
			classNum -= componant->paramCount();
		}
		else
		{
			std::string out(model.getModelStr());
			out.push_back('+');
			out.push_back(componant->getComponantChar());
			out.push_back('p');
			out.append(std::to_string(classNum));
			return out;
		}
	}
	return model.getModelStr();
}
