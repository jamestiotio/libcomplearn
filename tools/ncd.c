#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <dirent.h>

#include <complearn.h>
#include <clutil.h>

#include "ncdutil.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

static struct option long_options[] = {
{"compressor", required_argument, 0, 'c'},
{"info", no_argument, 0, 'i'},
{"option", required_argument, 0, 'o'},
{"square", no_argument, 0, 's'},
{"basic", no_argument, 0, 'b'},
{"rectangle", no_argument, 0, 'r'},
{"list-compressors", no_argument, 0, 'l'},
{"filename-list", no_argument, 0, 'f'},
{"help", no_argument, 0, 'h'},
{0, 0, 0, 0}
};

struct CLWorkContext {
  double cSizePoint;
  struct DoubleHolder cSizeLine[2];
  struct StringHolder labels[2];
  double **cSizeRect;
};

void printCompressedSize(double result);
void printNCD(double result);
struct CLDatum clDatumCat(struct CLDatum a, struct CLDatum b);

extern const char *NCDHelpMessage;

void printHelp(FILE *whichFp) {
  fprintf(whichFp, "%s", NCDHelpMessage);
}

struct NCDCommandLineOptions {
  char *compressor;
  char *compressor_options[256];
  int optionCount;
  int isSquare;
  int isBasic;
  int isRectangle;
  int isFilenameList;
  int isDefaultCommand;
  int isInfoCommand;
  int isListCompressorsCommand;
  int isHelpCommand;
};

void convertStringToIterator(struct NCDIterator *i, char *arg, struct NCDCommandLineOptions *ncdclo) {
  int argIsDir = isDir(arg);
  ncdiOpenIterator(i, arg, argIsDir ? NCDIteratorTypeDirectory :
    ncdclo->isFilenameList ?
       NCDIteratorTypeFilenameList :
       NCDIteratorTypeSingleFile  );
}
void doNCDMatrix(struct CLWorkContext *work,
                  struct NCDCommandLineOptions *cliopt,
                  struct CLCompressor comp,
                  struct CLCompressorConfig *clConfig,
             char *arg1, char *arg2) {
  struct NCDIterator i1;
  struct NCDIterator i2;
  if (arg1 == NULL) {
    fprintf(stderr, "Not enough arguments.\n");
    printHelp(stderr);
    exit(1);
  }
  if (cliopt->isSquare && arg2 == NULL) {
    arg2 = strdup(arg1);
  }
  if (arg2 == NULL) {
    fprintf(stderr, "Not enough arguments.\n");
    printHelp(stderr);
    exit(1);
  }
  convertStringToIterator(&i1, arg1, cliopt);
  convertStringToIterator(&i2, arg2, cliopt);
  for (;;) {
    int succeeded;
    struct CLRichDatum result = ncdiNextIterator(&i1, 0, &succeeded);
    if (succeeded) {
      pushDoubleHolder(&work->cSizeLine[0], comp.compressedSize(result.datum, clConfig));
      clFreeDatum(&result.datum);
    } else {
      break;
    }
  }
  ncdiCloseIterator(&i1);
  for (;;) {
    int succeeded;
    struct CLRichDatum result = ncdiNextIterator(&i2, 0, &succeeded);
    if (succeeded) {
      pushDoubleHolder(&work->cSizeLine[1], comp.compressedSize(result.datum, clConfig));
      clFreeDatum(&result.datum);
    } else {
      break;
    }
  }
  ncdiCloseIterator(&i2);
  convertStringToIterator(&i1, arg1, cliopt);
  int x = 0;
  int succeeded;
  for (;;) {
    struct CLRichDatum d1 = ncdiNextIterator(&i1, NCDNoLabels, &succeeded);
    if (!succeeded) {
      break;
    }
    convertStringToIterator(&i2, arg2, cliopt);
    int y = 0;
    for (;;) {
      struct CLRichDatum d2 = ncdiNextIterator(&i2, NCDNoLabels, &succeeded);
      if (!succeeded) {
        clFreeDatum(&d2.datum);
        break;
      }
      struct CLDatum input = clDatumCat(d1.datum, d2.datum);
      work->cSizePoint = comp.compressedSize(input, clConfig);
      clFreeDatum(&input);
      double cab = work->cSizePoint;
      double ca = getDoubleHolderData(&work->cSizeLine[0])[x];
      double cb = getDoubleHolderData(&work->cSizeLine[1])[y];
      double ncd = clNCD(ca, cb, cab);
      printNCD(ncd);
      printf(" ");
      clFreeDatum(&d2.datum);
      y += 1;
    }
    printf("\n");
    ncdiCloseIterator(&i2);
    clFreeDatum(&d1.datum);
    x += 1;
  }
  ncdiCloseIterator(&i1);
  exit(0);
}

void doBasicBytes(struct CLWorkContext *work,
                  struct NCDCommandLineOptions *cliopt,
                  struct CLCompressor comp,
                  struct CLCompressorConfig *clConfig,
             char *arg1, char *arg2) {
  struct NCDIterator i1;
  struct NCDIterator i2;
  if (arg1 == NULL) {
    fprintf(stderr, "Not enough arguments.\n");
    printHelp(stderr);
    exit(1);
  }
  if (arg2 == NULL) {
    convertStringToIterator(&i1, arg1, cliopt);
    for (;;) {
      int succeeded;
      struct CLRichDatum result = ncdiNextIterator(&i1, 0, &succeeded);
      if (succeeded) {
        work->cSizePoint = comp.compressedSize(result.datum, clConfig);
        clFreeDatum(&result.datum);
        printCompressedSize(work->cSizePoint);
        printf(" ");
      } else {
        break;
      }
    }
    printf("\n");
    exit(0);
  }
  convertStringToIterator(&i1, arg1, cliopt);
  int succeeded;
  for (;;) {
    struct CLRichDatum d1 = ncdiNextIterator(&i1, NCDNoLabels, &succeeded);
    if (!succeeded) {
      break;
    }
    convertStringToIterator(&i2, arg2, cliopt);
    for (;;) {
      struct CLRichDatum d2 = ncdiNextIterator(&i2, NCDNoLabels, &succeeded);
      if (!succeeded) {
        clFreeDatum(&d2.datum);
        break;
      }
      struct CLDatum input = clDatumCat(d1.datum, d2.datum);
      work->cSizePoint = comp.compressedSize(input, clConfig);
      clFreeDatum(&input);
      printCompressedSize(work->cSizePoint);
      printf(" ");
      clFreeDatum(&d2.datum);
    }
    printf("\n");
    ncdiCloseIterator(&i2);
    clFreeDatum(&d1.datum);
  }
  ncdiCloseIterator(&i1);
  exit(0);
}

struct CLDatum clDatumCat(struct CLDatum a, struct CLDatum b) {
  struct CLDatum r;
  r.length = a.length + b.length;
  r.data = malloc(r.length);
  memcpy(r.data, a.data, a.length);
  memcpy(r.data+a.length, b.data, b.length);
  return r;
}

void printNCD(double result) {
  if (result == floor(result) && result == ((double) ((int) result))) {
    printf("%d", (int) result);
  } else {
    printf("%f", result);
  }
}

void printCompressedSize(double result)
{
  if (result == floor(result) && result == ((double) ((int) result))) {
    printf("%d", (int) result);
  } else {
    printf("%f", result);
  }
}

int main(int argc, char **argv)
{
  struct NCDCommandLineOptions ncdclo;
  memset(&ncdclo, 0, sizeof(ncdclo));
  clInit();
  ncdclo.compressor = "zlib";
  ncdclo.isDefaultCommand = 1;
  int c;
  struct CLCompressor comp;
  if (clHasCompressor("xz")) {
    ncdclo.compressor = "xz";
  }

  opterr = 0;
  while (1) {
  int option_index = 0;
    c = getopt_long (argc, argv, "bc:fhilo:rs", long_options, &option_index);
    if (c == -1) {
      break;
    }

    switch (c)
      {
      case 'b':
        ncdclo.isBasic = 1;
        break;
      case 'c':
        ncdclo.compressor = strdup(optarg);
        break;
      case 'f':
        ncdclo.isFilenameList = 1;
        break;
      case 'h':
        ncdclo.isHelpCommand = 1;
        ncdclo.isDefaultCommand = 0;
        break;
      case 'i':
        ncdclo.isInfoCommand = 1;
        ncdclo.isDefaultCommand = 0;
        break;
      case 'l':
        ncdclo.isListCompressorsCommand = 1;
        ncdclo.isDefaultCommand = 0;
        break;
      case 'o':
        ncdclo.compressor_options[ncdclo.optionCount++] = strdup(optarg);
        break;
      case 'r':
        ncdclo.isRectangle = 1;
        ncdclo.isSquare = 0;
        break;
      case 's':
        ncdclo.isSquare = 1;
        ncdclo.isRectangle = 0;
        break;
      default:
        abort ();
      }
  }
  clInit();
  comp = clLoadCompressor(ncdclo.compressor);
  struct CLCompressorConfig clConfig;
  clConfig = clNewConfig();
  int i;
  for (i = 0; i < ncdclo.optionCount; i += 1) {
    char *cur = strdup(ncdclo.compressor_options[i]);
    char *fieldLeft = strtok(cur, "=");
    if (fieldLeft == NULL) { free(cur); continue; }
    char *fieldRight = strtok(NULL, "=");
    if (fieldRight == NULL) { fieldRight = ""; }
    comp.updateConfiguration(&clConfig, fieldLeft, fieldRight);
    free(cur);
  }
  if (ncdclo.isHelpCommand) {
    printHelp(stdout);
    exit(0);
  }
  if (ncdclo.isListCompressorsCommand) {
    char **clist;
    int count;
    clListCompressors(&clist, &count);
    printf("%d compressors:\n", count);
    int i;
    for (i = 0; i < count; ++i) {
      printf("%s\n", clist[i]);
    }
    printf("Default compressor: %s\n", ncdclo.compressor);
    exit(0);
  }
  char *firstAxis = NULL, *secondAxis = NULL;
  if (optind < argc) {
    firstAxis = argv[optind];
  }
  if (optind < argc-1) {
    secondAxis = argv[optind+1];
  }
  if (optind < argc-2) {
    fprintf(stderr, "Error: too many arguments\n");
    exit(1);
  }
  struct CLWorkContext work;
  newDoubleHolder(&work.cSizeLine[0]);
  newDoubleHolder(&work.cSizeLine[1]);
  newStringHolder(&work.labels[0]);
  newStringHolder(&work.labels[1]);
  if (ncdclo.isBasic) {
    doBasicBytes(&work, &ncdclo, comp, &clConfig, firstAxis, secondAxis);
    exit(0);
  } else {
    doNCDMatrix(&work, &ncdclo, comp, &clConfig, firstAxis, secondAxis);
    exit(0);
  }
  exit(0);
}
