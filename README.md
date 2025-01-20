# KissDatasetGenerator

KissDatasetGenerator creates dataset tarballs for TorchKissAnn
KissDatasetGenerator uses eisgenerator to create synthetic datasets and can also create datasets from real data of various types
KissDatasetGenerator supports down-selecting data as well as applying transformations sutch as those from libeisnoise and libeisdrt.

## Requirements

* You must be running unix or a unix-like os such as linux
* A c++17 compiler like gcc
* cmake
* pkg-config
* libeisgenerator
* libkisstype
* libeisdrt
* libeisnoise

## Build and instructions

To build and install this tool do the following:

```
$ git clone REPO_URL dataformaters
$ cd dataformaters
$ mkdir build
$ cd build
$ cmake ..
$ make
# make install
```

## Usage

```
Usage: kissdatasetgenerator [OPTION...] 
Application that checkes and conditions a dataset in an eis dir

  -a, --archive              save as a tar archive instead of a directory
  -c, --frequency-count=[NUMBER]   the number of frequencies to simmulate,
                             default: 100
  -d, --dataset=[PATH]       input dataset to use or the model string, in case
                             of the regression purpose
  -g, --no-negative          remove examples with negative labels from the
                             dataset
  -l, --select-labels[=[LABLE1,LABEL2,...]]
                             select thiese labels to appear in the output
                             dataset (requires them to be present in the
                             input)
  -n, --no-normalization     turn off normalization on dataset types that
                             ususally do this
  -o, --out-dir=[PATH]       directory where to export dataset
  -p, --test-percent=[NUMBER]   test dataset percentage
  -q, --quiet                Show only errors
  -r, --frequency-range=[RANGE]   Frequency range to simulate for simulated
                             datasets
  -s, --size=[NUMBER]        size the dataset should have
  -t, --type=[TYPE]          type of dataset to export valid types: gen,
                             gennoise, passfail, regression, dir, tar
  -v, --verbose              Show debug messages
  -x, --extra-inputs[=[INPUT1,INPUT2,...]]
                             select thiese labels to appear in the output
                             dataset as extra inputs (requires them to be
                             present in the input)
  -?, --help                 Give this help list
      --usage                Give a short usage message
  -V, --version              Print program version

Mandatory or optional arguments to long options are also mandatory or optional
for any corresponding short options.

Report bugs to <carl@uvos.xyz>.
```

For example to generate a synthetic parameter regression dataset one could use:

`kissdatasetgenerator -t regression -d "r-rc" -a -o r_rc -p 10 -c 20`

which will result in the files `r_rc_test.tar` and `r_rc_train.tar` 

