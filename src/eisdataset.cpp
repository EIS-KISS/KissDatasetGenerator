#include "eisdataset.h"

eis::EisSpectra EisDataset::get(size_t index)
{
	eis::EisSpectra data = getImpl(index);
	return data;
}
