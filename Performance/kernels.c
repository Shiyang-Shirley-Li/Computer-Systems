/*******************************************
 * Solutions for the CS:APP Performance Lab
 ********************************************/

#include <stdio.h>
#include <stdlib.h>
#include "defs.h"

/* 
 * Please fill in the following student struct 
 */
student_t student = {
  "Shiyang(Shirley) Li",     /* Full name */
  "u1160160@umail.utah.edu",  /* Email address */
};

/***************
 * COMPLEX KERNEL
 ***************/

/******************************************************
 * Your different versions of the complex kernel go here
 ******************************************************/

/* 
 * naive_complex - The naive baseline version of complex 
 */
char naive_complex_descr[] = "naive_complex: Naive baseline implementation";
void naive_complex(int dim, pixel *src, pixel *dest)
{
  int i, j;

  for(i = 0; i < dim; i++)
    for(j = 0; j < dim; j++)
    {

      dest[RIDX(dim - j - 1, dim - i - 1, dim)].red = ((int)src[RIDX(i, j, dim)].red +
						      (int)src[RIDX(i, j, dim)].green +
						      (int)src[RIDX(i, j, dim)].blue) / 3;
      
      dest[RIDX(dim - j - 1, dim - i - 1, dim)].green = ((int)src[RIDX(i, j, dim)].red +
							(int)src[RIDX(i, j, dim)].green +
							(int)src[RIDX(i, j, dim)].blue) / 3;
      
      dest[RIDX(dim - j - 1, dim - i - 1, dim)].blue = ((int)src[RIDX(i, j, dim)].red +
						       (int)src[RIDX(i, j, dim)].green +
						       (int)src[RIDX(i, j, dim)].blue) / 3;

    }
}


/* 
 * complex - Your current working version of complex
 * IMPORTANT: This is the version you will be graded on
 */
char complex_descr[] = "complex: Current working version";
void complex(int dim, pixel *src, pixel *dest)
{
  int i, j, k, m;//k, m for index in small blocks
  int block_size = dim >> 4;//the dimension of small blocks
  int dim_1 = dim - 1;//the same all the time, so we can put it outside the loop

  //Divide one image into some number of small blocks, which can work at the same time
  for(i = 0; i < dim; i += block_size)
  {
    for(j = 0; j < dim; j += block_size)
    {
      for(k = i; k < i + block_size; k++)
      {
        int dim_k_1 = dim_1 - k;//outside m loop to avoid calculating j + block_size times 
        for(m = j; m < j + block_size; m++)
        {
          int dim_m_1 = dim_1 -m;
          int dest_index = RIDX(dim_k_1, dim_m_1, dim);
          int src_index = RIDX(m, k, dim);

          unsigned short average = ((int)src[src_index].red + (int)src[src_index].green + (int)src[src_index].blue) / 3;
          dest[dest_index].red = dest[dest_index].green = dest[dest_index].blue = average;       
        }
      }
    }

  }
}

/*********************************************************************
 * register_complex_functions - Register all of your different versions
 *     of the complex kernel with the driver by calling the
 *     add_complex_function() for each test function. When you run the
 *     driver program, it will test and report the performance of each
 *     registered test function.  
 *********************************************************************/

void register_complex_functions() {
  add_complex_function(&complex, complex_descr);
  add_complex_function(&naive_complex, naive_complex_descr);
}


/***************
 * MOTION KERNEL
 **************/

/***************************************************************
 * Various helper functions for the motion kernel
 * You may modify these or add new ones any way you like.
 **************************************************************/


/* 
 * weighted_combo - Returns new pixel value at (i,j) 
 */
static pixel weighted_combo(int dim, int i, int j, pixel *src) 
{
  int ii, jj;
  pixel current_pixel;

  int red, green, blue;
  red = green = blue = 0;

  int num_neighbors = 0;
  for(ii=0; ii < 3; ii++)
    for(jj=0; jj < 3; jj++) 
      if ((i + ii < dim) && (j + jj < dim)) 
      {
	num_neighbors++;
	red += (int) src[RIDX(i+ii,j+jj,dim)].red;
	green += (int) src[RIDX(i+ii,j+jj,dim)].green;
	blue += (int) src[RIDX(i+ii,j+jj,dim)].blue;
      }
  
  current_pixel.red = (unsigned short) (red / num_neighbors);
  current_pixel.green = (unsigned short) (green / num_neighbors);
  current_pixel.blue = (unsigned short) (blue / num_neighbors);
  
  return current_pixel;
}



/******************************************************
 * Your different versions of the motion kernel go here
 ******************************************************/


/*
 * naive_motion - The naive baseline version of motion 
 */
char naive_motion_descr[] = "naive_motion: Naive baseline implementation";
void naive_motion(int dim, pixel *src, pixel *dst) 
{
  int i, j;
    
  for (i = 0; i < dim; i++)
    for (j = 0; j < dim; j++)
      dst[RIDX(i, j, dim)] = weighted_combo(dim, i, j, src);
}

/*
 * weighted_combo_a_b(dim, i, j, src) - Returns new pixel value at (i,j) for a*b block 
 */
__attribute__((always_inline)) static pixel weighted_combo_3_3(int dim, int i, int j, pixel *src)
{
  pixel current_pixel;

  int red, green, blue;
  red = green = blue = 0;

  //(i, j); (i, j+1); (i, j+2)
  int i_j = RIDX(i, j, dim);
  int i_j_1 = i_j + 1;
  int i_j_2 =  i_j_1 + 1;

  red += (int) src[i_j].red;
	green += (int) src[i_j].green;
	blue += (int) src[i_j].blue;

  red += (int) src[i_j_1].red;
	green += (int) src[i_j_1].green;
	blue += (int) src[i_j_1].blue;

  red += (int) src[i_j_2].red;
	green += (int) src[i_j_2].green;
	blue += (int) src[i_j_2].blue;
  
  //(i+1, j); (i+1, j+1); (i+1; j+2)
  int i_1_j = RIDX(i+1, j, dim);
  int i_1_j_1 = i_1_j + 1;
  int i_1_j_2 =  i_1_j_1 + 1;

  red += (int) src[i_1_j].red;
	green += (int) src[i_1_j].green;
	blue += (int) src[i_1_j].blue;

  red += (int) src[i_1_j_1].red;
	green += (int) src[i_1_j_1].green;
	blue += (int) src[i_1_j_1].blue;

  red += (int) src[i_1_j_2].red;
	green += (int) src[i_1_j_2].green;
	blue += (int) src[i_1_j_2].blue;

  //(i+2, j); (i+2, j+2); (i+2, j+2)
  int i_2_j = RIDX(i+2, j, dim);
  int i_2_j_1 = i_2_j + 1;
  int i_2_j_2 =  i_2_j_1 + 1;

  red += (int) src[i_2_j].red;
	green += (int) src[i_2_j].green;
	blue += (int) src[i_2_j].blue;

  red += (int) src[i_2_j_1].red;
	green += (int) src[i_2_j_1].green;
	blue += (int) src[i_2_j_1].blue;

  red += (int) src[i_2_j_2].red;
	green += (int) src[i_2_j_2].green;
	blue += (int) src[i_2_j_2].blue;

  current_pixel.red = (unsigned short) (red / 9);
  current_pixel.green = (unsigned short) (green / 9);
  current_pixel.blue = (unsigned short) (blue / 9);
  
  return current_pixel;
}

__attribute__((always_inline)) static pixel weighted_combo_3_2(int dim, int i, int j, pixel *src)
{
  int k, m;
  pixel current_pixel;

  int red, green, blue;
  red = green = blue = 0;

  for (k = 0; k < 3; k++)
  {
    int ii = k + i;
    for (m = 0; m < 2; m++)
    {
      pixel *source = &src[RIDX(ii, m + j, dim)];
      red += (int)source->red;
      green += (int)source->green;
      blue += (int)source->blue;
    }
  }

  current_pixel.red = (unsigned short)(red / 6);
  current_pixel.green = (unsigned short)(green / 6);
  current_pixel.blue = (unsigned short)(blue / 6);
  return current_pixel;
}

__attribute__((always_inline)) static pixel weighted_combo_2_3(int dim, int i, int j, pixel *src)
{
  int k, m;
  pixel current_pixel;

  int red, green, blue;
  red = green = blue = 0;

  for (k = 0; k < 2; k++)
  {
    int ii = k + i;
    for (m = 0; m < 3; m++)
    {
      pixel *source = &src[RIDX(ii, m + j, dim)];
      red += (int)source->red;
      green += (int)source->green;
      blue += (int)source->blue;
    }
  }

  current_pixel.red = (unsigned short)(red / 6);
  current_pixel.green = (unsigned short)(green / 6);
  current_pixel.blue = (unsigned short)(blue / 6);
  return current_pixel;
}

__attribute__((always_inline)) static pixel weighted_combo_2_2(int dim, int i, int j, pixel *src)
{
  int k, m;
  pixel current_pixel;

  int red, green, blue;
  red = green = blue = 0;

  for(k = 0; k < 2; k++)
  {
    int ii = k + i;
    for(m = 0; m < 2; m++) 
    {
      pixel src_pixel = src[RIDX(ii,m + j, dim)];
	    red += (int) src_pixel.red;
	    green += (int) src_pixel.green;
	    blue += (int) src_pixel.blue;
    }
  }
  current_pixel.red = (unsigned short) (red / 4);
  current_pixel.green = (unsigned short) (green / 4);
  current_pixel.blue = (unsigned short) (blue / 4);
  
  return current_pixel;
}

__attribute__((always_inline)) static pixel weighted_combo_1_2(int dim, int i, int j, pixel *src)
{
  pixel current_pixel;

  int red, green, blue;
  red = green = blue = 0;

  for(j; j < dim; j++) 
  {
    pixel src_pixel = src[RIDX(i,j,dim)];
	  red += (int) src_pixel.red;
	  green += (int) src_pixel.green;
	  blue += (int) src_pixel.blue;
  }
  
  current_pixel.red = (unsigned short) (red / 2);
  current_pixel.green = (unsigned short) (green / 2);
  current_pixel.blue = (unsigned short) (blue / 2);
  
  return current_pixel;
}

__attribute__((always_inline)) static pixel weighted_combo_2_1(int dim, int i, int j, pixel *src)
{
  pixel current_pixel;

  int red, green, blue;
  red = green = blue = 0;

  for(i; i < dim; i++)
  {
    pixel src_pixel = src[RIDX(i,j,dim)];
	  red += (int) src_pixel.red;
	  green += (int) src_pixel.green;
	  blue += (int) src_pixel.blue;
  }
  
  current_pixel.red = (unsigned short) (red / 2);
  current_pixel.green = (unsigned short) (green / 2);
  current_pixel.blue = (unsigned short) (blue / 2);
  
  return current_pixel;
}

__attribute__((always_inline)) static pixel weighted_combo_1_3(int dim, int i, int j, pixel *src)
{
  pixel current_pixel;

  int red, green, blue;
  red = green = blue = 0;
  int jj = j + 3;

  for(j; j < jj; j++) 
  {
    pixel src_pixel = src[RIDX(i,j,dim)];
	  red += (int) src_pixel.red;
	  green += (int) src_pixel.green;
	  blue += (int) src_pixel.blue;
  }
  
  current_pixel.red = (unsigned short) (red / 3);
  current_pixel.green = (unsigned short) (green / 3);
  current_pixel.blue = (unsigned short) (blue / 3);
  
  return current_pixel;
}

__attribute__((always_inline)) static pixel weighted_combo_3_1(int dim, int i, int j, pixel *src)
{
  pixel current_pixel;

  int red, green, blue;
  red = green = blue = 0;
  int ii = i + 3;

  for(i; i < ii; i++) 
  {
    pixel src_pixel = src[RIDX(i,j,dim)];
	  red += (int) src_pixel.red;
	  green += (int) src_pixel.green;
	  blue += (int) src_pixel.blue;
  }
  
  current_pixel.red = (unsigned short) (red / 3);
  current_pixel.green = (unsigned short) (green / 3);
  current_pixel.blue = (unsigned short) (blue / 3);
  
  return current_pixel;
}

/*
 * motion - Your current working version of motion. 
 * IMPORTANT: This is the version you will be graded on
 */
char motion_descr[] = "motion: Current working version";
void motion(int dim, pixel *src, pixel *dst) 
{
  int i, j;
  int dim_1 = dim - 1;
  int dim_2 = dim - 2;

  //For complete 3 * 3 blocks  
  for (i = 0; i < dim_2; i++)
    for (j = 0; j < dim_2; j++)
      dst[RIDX(i, j, dim)] = weighted_combo_3_3(dim, i, j, src);

  //For incomplete 2 * 3 blocks
  for(j = 0; j < dim_2; j++)
    dst[RIDX(dim_2, j, dim)] = weighted_combo_2_3(dim, dim_2, j, src);
  
  //For incomplete 3 * 2 blocks
  for( i = 0; i < dim_2; i++)
    dst[RIDX(i, dim_2, dim)] = weighted_combo_3_2(dim, i, dim_2, src);

  //For incomplete 1 * 3 blocks
  for(j = 0;j < dim_2; j++)
    dst[RIDX(dim_1, j, dim)] = weighted_combo_1_3(dim, dim_1, j, src);

  //For incomplete 3 * 1 blocks
  for(i = 0; i < dim_2; i++)
    dst[RIDX(i, dim_1, dim)] = weighted_combo_3_1(dim, i, dim_1, src);
  
  //For incomplete 2 * 2 blocks
  dst[RIDX(dim_2, dim_2, dim)] = weighted_combo_2_2(dim, dim_2, dim_2, src);
  
  //For incomplete 1 * 2 blocks
  dst[RIDX(dim_1, dim_2, dim)] = weighted_combo_1_2(dim, dim_1, dim_2, src);

  //For incomplete 2 * 1 blocks
  dst[RIDX(dim_2, dim_1, dim)] = weighted_combo_2_1(dim, dim_2, dim_1, src);

  //For incomplete 1 * 1 blocks
  dst[RIDX(dim_1, dim_1, dim)] = src[RIDX(dim_1, dim_1, dim)];  

}

/********************************************************************* 
 * register_motion_functions - Register all of your different versions
 *     of the motion kernel with the driver by calling the
 *     add_motion_function() for each test function.  When you run the
 *     driver program, it will test and report the performance of each
 *     registered test function.  
 *********************************************************************/

void register_motion_functions() {
  add_motion_function(&motion, motion_descr);
  add_motion_function(&naive_motion, naive_motion_descr);
}
