/*****************************************************************************\
* RunNewBorders.c
*
* AUTHOR  : Lucca Lacerda
* DATE    : 2024-04-08
* LICENSE : MIT License
* EMAIL   : lsplacerda@sga.pucminas.br
\*****************************************************************************/
#include "ift.h"
#include "iftArgs.h"

void usage();
iftImage *findError
(iftImage *label_img, long X_value, long Y_value);

void readImgInputs
(iftArgs *args, iftImage **img, iftImage **labels, iftCSV **csv, const char **path, 
  bool *is_video);
void getCSVCoord(const iftCSV *csv, const char *label, long *X_value, long *Y_value);

int main(int argc, char const *argv[])
{
  //-------------------------------------------------------------------------//
  bool has_req, has_help;
  iftArgs *args;

  args = iftCreateArgs(argc, argv);

  has_req = iftExistArg(args, "labels") && iftExistArg(args, "out") && iftExistArg(args, "file");
  has_help = iftExistArg(args, "help");

  if(!has_req || has_help)
  {
    usage(); 
    iftDestroyArgs(&args);
    return EXIT_FAILURE;
  }
  //-------------------------------------------------------------------------//
  bool is_video;
  const char *OUT_PATH;
  iftImage *img, *label_img, *out_img;
  iftCSV *csv;
  long X_value;
  long Y_value;

  readImgInputs(args, &img, &label_img, &csv, &OUT_PATH, &is_video);
  getCSVCoord(csv, "Superpixel", &X_value, &Y_value);  
//   printf("X = %ld, Y = %ld\n", X_value, Y_value);
  iftDestroyArgs(&args);

  out_img = findError(label_img, X_value, Y_value);
  iftDestroyImage(&label_img);

  if(is_video == false)
  { iftWriteImageByExt(out_img, OUT_PATH); }
  else
  { iftWriteVolumeAsSingleVideoFolder(out_img, OUT_PATH); }
  iftDestroyImage(&out_img);
		    // if (csv == NULL) return;
			// // Check and print header
			// if (csv->header != NULL) {
			// 	for (long col = 0; col < csv->ncols; ++col) {
			// 		printf("%s\t", csv->header[col]);
			// 	}
			// 	printf("\n");
			// }

			// // Print data
			// for (long row = 0; row < csv->nrows; ++row) {
			// 	for (long col = 0; col < csv->ncols; ++col) {
			// 		printf("%s\t", csv->data[row][col]);
			// 	}
			// 	printf("\n");
			// }

  return EXIT_SUCCESS;
}

void usage()
{
  const int SKIP_IND = 15; // For indentation purposes
  printf("\nThe required parameters are:\n");
  printf("%-*s %s\n", SKIP_IND, "--labels", 
         "Input label image");
  printf("%-*s %s\n", SKIP_IND, "--out", 
         "Output pseudo colored label image ");
  
  printf("%-*s %s\n", SKIP_IND, "--file", 
         "File with the superpixel ");

//   printf("\nThe optional parameters are:\n");
//   printf("%-*s %s\n", SKIP_IND, "--img", 
//          "Original image");
//   printf("%-*s %s\n", SKIP_IND, "--opac", 
//          "Label opacity. Default: 1.0");
//   printf("%-*s %s\n", SKIP_IND, "--help", 
//          "Prints this message");

  printf("\n");
}

iftImage *findError
(iftImage *label_img, long X_value, long Y_value)
{
  #if IFT_DEBUG //-----------------------------------------------------------//
  // assert(label_img != NULL);
  // if(label_img != NULL) iftVerifyImageDomains(label_img, label_img, __func__);
  // assert(alpha >= 0 && alpha <= 1);
  #endif //------------------------------------------------------------------//
  // const iftColor BLACK_RGB = {{0,0,0},1.0};
  iftImage *out_img;
  //  *tmp_img;

  out_img = iftCreateImage(label_img->xsize, label_img->ysize, 
                                   label_img->zsize);

  iftVoxel coord;
  coord.x = X_value;
  coord.y = Y_value;
  coord.z = 0;
//   printf("%d\n",label_img->val[iftGetVoxelIndex(label_img,coord)]);
  
int label = label_img->val[iftGetVoxelIndex(label_img,coord)];

    for (int p = 0; p < out_img->n; p++) {
        if (label_img->val[p] == label)
          out_img->val[p] = 1;
        else
            out_img->val[p] = 0;
    }

  return out_img;
}

void readImgInputs
(iftArgs *args, iftImage **img, iftImage **labels, iftCSV **csv, const char **path, 
  bool *is_video)
{
  #if IFT_DEBUG //-----------------------------------------------------------//
  assert(args != NULL);
  assert(labels != NULL);
  assert(path != NULL);
  #endif //------------------------------------------------------------------//
  const char *PATH;

  if(iftHasArgVal(args, "labels") == true) 
  {
    PATH = iftGetArg(args, "labels");

    if(iftIsImageFile(PATH) == true) 
    { (*labels) = iftReadImageByExt(PATH); (*is_video) = false; }
    else if(iftDirExists(PATH) == true)
    { (*labels) = iftReadImageFolderAsVolume(PATH); (*is_video) = true;}
    else { iftError("Unknown image/video format", __func__); } 
  }
  else iftError("No label image path was given", __func__);

  if(iftHasArgVal(args, "out") == true)
  { (*path) = iftGetArg(args, "out"); }
  else iftError("No output image path was given", __func__);

  if(iftExistArg(args, "img") == true)
  {
    if(iftHasArgVal(args, "img") == true) 
    {
      PATH = iftGetArg(args, "img");

      if(iftIsImageFile(PATH) == true) 
      { (*img) = iftReadImageByExt(PATH); (*is_video) = false; }
      else if(iftDirExists(PATH) == true)
      { (*img) = iftReadImageFolderAsVolume(PATH); (*is_video) = true;}
      else { iftError("Unknown image/video format", __func__); } 
    }
    else iftError("No original image path was given", __func__);
  }
  else (*img) = NULL;
  if(iftExistArg(args,"file") == true){
    if(iftHasArgVal(args,"file") == true){
		const char *VAL = iftGetArg(args,"file");
		*csv = iftReadCSV(VAL,';');

    }
  }
}

void getCSVCoord(const iftCSV *csv, const char *value, long *X_value, long *Y_value) {

    if (csv == NULL || value == NULL || X_value == NULL || Y_value == NULL) return;

    // Search for the value in the CSV data
    for (long row = 0; row < csv->nrows; row++) {
        if (strcmp(csv->data[row][0], value) == 0) {
            // If the value is found, extract X and Y values from the next cell in the same row
            char *x_string = csv->data[row][1];
            char *y_string = csv->data[row][2];
            
            *X_value = strtol(x_string, NULL, 10); // Convert substring to long integer
            *Y_value = strtol(y_string, NULL, 10); // Convert substring to long integer
            
            return; // Extraction successful
        }
    }
    // Value not found in the CSV data
    printf("Value '%s' not found in the CSV data.\n", value);
}