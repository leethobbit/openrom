/*
 * Runes of Magic protocol analysis - protocol definition generation utility
 * Copyright (C) 2013-2015 Rink Springer <rink@rink.nu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License version 3
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <assert.h>
#include <err.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include "rompack.h"
#include "dataannotation.h"
#include "datatransformation.h"
#include "protocoldefinition.h"
#include "protocolcodegenerator.h"

// XXX Only to register the transformation/annotation so we won't reject files using them
class DummyTransformation : public XDataTransformation {
public:
	virtual bool Apply(const uint8_t* pSource, int iSourceLen, uint8_t* pDest, int& oDestLen) { return false; }
	virtual int EstimateBufferSize(const uint8_t* pSource, int iSourceLen) { return 0; }
};
class DummyAnnotation : public XDataAnnotation {
	virtual const char* Lookup(uint32_t v) { return "?"; }
};

static void
write_header(FILE* f)
{
	fprintf(f, "/* This file is automatically generated by mkdef - do not edit! */\n");
}

static void
usage(const char* progname)
{	
	fprintf(stderr, "usage: %s [-h?] [-v version] -d protocol.xml -c file.cc -p file.cc -i file.h\n", progname);
	fprintf(stderr, "\n");
	fprintf(stderr, "  -h, -?             this help\n");
	fprintf(stderr, "  -d protocol.xml    use supplied protocol definitions\n");
	fprintf(stderr, "  -v version         protocol version to use (default is latest)\n");
	fprintf(stderr, "  -c file.cc         write c++ code to file.cc\n");
	fprintf(stderr, "  -p file.cc         write python wrappers to file.cc\n");
	fprintf(stderr, "  -i file.h          write header file to file.h\n");
	fprintf(stderr, "\n");
}

int
main(int argc, char** argv)
{
	ProtocolDefinition oProtocolDef;
	oProtocolDef.RegisterTransformation("rompack", *new DummyTransformation);
	oProtocolDef.RegisterAnnotation("sys_name", *new DummyAnnotation);
	oProtocolDef.RegisterAnnotation("stat_name", *new DummyAnnotation);
	oProtocolDef.RegisterAnnotation("objectid", *new DummyAnnotation);
	oProtocolDef.RegisterAnnotation("charid", *new DummyAnnotation);

	int iVersion = -1;
	char* sCPPFile = NULL;
	char* sPythonCPPFile = NULL;
	char* sHFile = NULL;
	char* sProtocolDefFile = NULL;
	{
		int opt;
		while ((opt = getopt(argc, argv, "?hd:i:c:v:p:")) != -1) {
			switch(opt) {
				case 'd':
					sProtocolDefFile = optarg;
					break;
				case 'c':
					sCPPFile = optarg;
					break;
				case 'i':
					sHFile = optarg;
					break;
				case 'h':
				case '?':
				default:
					usage(argv[0]);
					return EXIT_FAILURE;
				case 'p':
					sPythonCPPFile = optarg;
					break;
				case 'v': {
					char* ptr;
					iVersion = (int)strtol(optarg, &ptr, 10);
					if (*ptr != '\0')
						errx(1, "version '%s' cannot be parsed", optarg);
					break;
				}
			}
		}
	}

	if (sProtocolDefFile == NULL || sCPPFile == NULL || sHFile == NULL || sPythonCPPFile == NULL) {
		fprintf(stderr, "%s: missing required arguments\n", argv[0]);
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	if (!oProtocolDef.Load(sProtocolDefFile, iVersion))
		errx(1, "can't load protocol definitions");

	FILE* pCFile = fopen(sCPPFile, "wt");
	if (pCFile == NULL)
		err(1, "can't create '%s'", sCPPFile);
	FILE* pHFile = fopen(sHFile, "wt");
	if (pHFile == NULL)
		err(1, "can't create '%s'", sHFile);
	FILE* pPythonCFile = fopen(sPythonCPPFile, "wt");
	if (pPythonCFile == NULL)
		err(1, "can't create '%s'", sPythonCPPFile);

	ProtocolCodeGenerator oGenerator(oProtocolDef);

	// Header file
	{
		write_header(pHFile);
		fprintf(pHFile, "#ifndef __ROMPACKET_H__\n");
		fprintf(pHFile, "#define __ROMPACKET_H__\n");
		fprintf(pHFile, "#include <stdint.h>\n");
		fprintf(pHFile, "#include \"romstructs.h\"\n");
		fprintf(pHFile, "\n");
		fprintf(pHFile, "class State;\n");
		fprintf(pHFile, "\n");
		fprintf(pHFile, "namespace ROMPacket {\n");
		fprintf(pHFile, "\n");
		fprintf(pHFile, "typedef uint8_t u8;\n");
		fprintf(pHFile, "typedef uint16_t u16;\n");
		fprintf(pHFile, "typedef uint32_t u32;\n");
		fprintf(pHFile, "typedef uint32_t unixtime;\n");
		fprintf(pHFile, "typedef int8_t s8;\n");
		fprintf(pHFile, "typedef int16_t s16;\n");
		fprintf(pHFile, "typedef int32_t s32;\n");
		fprintf(pHFile, "typedef uint32_t ulength;\n");
		fprintf(pHFile, "#define PACKED __attribute__((packed))\n");
		fprintf(pHFile, "\n");
		oGenerator.GenerateEnumerations(pHFile);
		oGenerator.GenerateTypes(pHFile);
		oGenerator.GeneratePackets(pHFile);
		oGenerator.GenerateParserClass(pHFile);
		fprintf(pHFile, "} /* namespace ROMPacket */\n");
		fprintf(pHFile, "#endif /* __ROMPACKET_H__ */\n");
	}

	// Source file
	{
		write_header(pCFile);
		fprintf(pCFile, "#include \"%s\"\n", sHFile);
		fprintf(pCFile, "#include <assert.h>\n");
		fprintf(pCFile, "#include <string.h> // for memset()\n");
		fprintf(pCFile, "#include <stdio.h>\n");
		fprintf(pCFile, "#include \"state.h\"\n");
		fprintf(pCFile, "#include \"../lib/rompack.h\"\n");
		fprintf(pCFile, "\n");
		fprintf(pCFile, "using namespace ROMPacket;\n");
		fprintf(pCFile, "typedef ROMPack rompack;\n");
		fprintf(pCFile, "\n");
		oGenerator.GenerateFunctions(pCFile);
		oGenerator.GenerateParser(pCFile, "m_Packet");
	}

	// Python file
	{
		oGenerator.GeneratePythonBindings(pPythonCFile);
	}

	fclose(pHFile);
	fclose(pCFile);
	fclose(pPythonCFile);
	return EXIT_SUCCESS;
}

/* vim:set ts=2 sw=2: */
