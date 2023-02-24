#pragma once
#include <string>
#include <vector>
#include <argp.h>
#include <iostream>
#include <filesystem>
#include "log.h"

const char *argp_program_version = "conditioneisdirdataset";
const char *argp_program_bug_address = "<carl@uvos.xyz>";
static char doc[] = "Application that checkes and conditions a dataset in an eis dir";
static char args_doc[] = "";

static struct argp_option options[] =
{
  {"verbose",		'v', 0,			0,	"Show debug messages" },
  {"quiet",			'q', 0,			0,	"Show only errors" },
  {"dataset", 		'd', "[PATH]",	0,	"dataset to use" },
  {"out-dir",		'o', "[PATH]",	0,	"directory where to export cleaned dataset"},
  {"test-percent",	't', "[NUMBER]",0,	"test dataset percentage"},
  {"no-balance",	'n', 0,			0,	"dont balance data with generated examples"},
  { 0 }
};

struct Config
{
	std::filesystem::path datasetPath;
	std::filesystem::path outDir = "./out";
	bool noBalance = false;
	int testPercent = 0;
};

static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
	Config *config = reinterpret_cast<Config*>(state->input);

	try
	{
		switch (key)
		{
		case 'q':
			Log::level = Log::ERROR;
			break;
		case 'v':
			Log::level = Log::DEBUG;
			break;
		case 'd':
			config->datasetPath = arg;
			break;
		case 'o':
			config->outDir = arg;
			break;
		case 'n':
			config->noBalance = true;
			break;
		case 't':
			config->testPercent = std::stoi(std::string(arg));
			if(config->testPercent < 0 || config->testPercent > 100)
			{
				std::cout<<arg<<" passed for argument -"<<key<<" is not a valid percentage.";
				return ARGP_KEY_ERROR;
			}
			break;
		default:
			return ARGP_ERR_UNKNOWN;
		}
	}
	catch(const std::invalid_argument& ex)
	{
		std::cout<<arg<<" passed for argument -"<<key<<" is not a valid number.";
		return ARGP_KEY_ERROR;
	}
	return 0;
}

static struct argp argp = {options, parse_opt, args_doc, doc};
