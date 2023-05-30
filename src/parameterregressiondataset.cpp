#include <eisdrt/eisdrt.h>
#include <eisdrt/types.h>
#include <complex>
#include <eisgenerator/eistype.h>

#include "parameterregressiondataset.h"
#include "log.h"

ParameterRegressionDataset::ParameterRegressionDataset(const std::string& modelStr, int64_t desiredSize, int64_t outputSize, double noiseI, bool drtI):
model(modelStr), omega(10, 1e6, drtI ? outputSize : outputSize/2, true), noise(noiseI), drt(drtI)
{
	sweepCount = model.getRequiredStepsForSweeps();
	model.setParamSweepCountClosestTotal(desiredSize);
	sweepCount = model.getRequiredStepsForSweeps();
	parameterCount = model.getParameterCount();
}

bool ParameterRegressionDataset::isMulticlass()
{
	return true;
}

eis::EisSpectra ParameterRegressionDataset::getImpl(size_t index)
{
	std::vector<eis::DataPoint> data = model.executeSweep(omega, index);
	std::vector<fvalue> parameters = model.getFlatParameters();

	assert(data.size());
	if(drt)
	{
		FitMetics fm;
		try {
			std::vector<fvalue> drt = calcDrt(data, fm, FitParameters(1000));
			std::vector<fvalue> omegas = omega.getRangeVector();
			assert(drt.size() == omegas.size());
			data.assign(drt.size(), eis::DataPoint());
			for(size_t i = 0; i < drt.size(); ++i)
			{
				data[i].im = std::complex<fvalue>(drt[i], 0);
				data[i].omega = omegas[i];
			}
		}
		catch (const drt_errror& ex)
		{
			Log(Log::DEBUG)<<"Drt calculation failed! using next index";
			index++;
			if(index >= size())
				index = 0;
			return getImpl(index);
		}
	}

	return eis::EisSpectra(data, model.getModelStr(), "", parameters);
}

size_t ParameterRegressionDataset::classesCount() const
{
	return parameterCount;
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
			classNum -= componant->paramCount();
		else
			return std::to_string(componant->getComponantChar()) + "p" + std::to_string(classNum);
	}
	return model.getModelStr();
}
