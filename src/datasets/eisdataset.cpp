#include "eisdataset.h"

eis::Spectra EisDataset::get(size_t index)
{
	eis::Spectra data = getImpl(index);
	return data;
}
