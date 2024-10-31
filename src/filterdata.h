#pragma once

#include <eisgenerator/normalize.h>
#include <eisgenerator/basicmath.h>
#include <kisstype/type.h>

inline void filterData(std::vector<eis::DataPoint>& data, size_t outputSize, bool normalize)
{
	if(normalize)
	{
		data = eis::reduceRegion(data);

		if(data.size() < outputSize/8)
		{
			data = std::vector<eis::DataPoint>();
			return;
		}
	}
	data = eis::rescale(data, outputSize/2);
}
