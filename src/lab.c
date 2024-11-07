


#include <stdlib.h>
#include <sys/time.h> /* for gettimeofday system call */
#include "lab.h"
#include <stdio.h>

//threading
#include <pthread.h>

/**
 * @brief Standard insertion sort that is faster than merge sort for small array's
 *
 * @param A The array to sort
 * @param p The starting index
 * @param r The ending index
 */
static void insertion_sort(int A[], int p, int r)
{
  int j;

  for (j = p + 1; j <= r; j++)
    {
      int key = A[j];
      int i = j - 1;
      while ((i > p - 1) && (A[i] > key))
        {
	  A[i + 1] = A[i];
	  i--;
        }
      A[i + 1] = key;
    }
}


void mergesort_s(int A[], int p, int r)
{
  if (r - p + 1 <=  INSERTION_SORT_THRESHOLD)
    {
      insertion_sort(A, p, r);
    }
  else
    {
      int q = (p + r) / 2;
      mergesort_s(A, p, q);
      mergesort_s(A, q + 1, r);
      merge_s(A, p, q, r);
    }

}

void merge_s(int A[], int p, int q, int r)
{
  int *B = (int *)malloc(sizeof(int) * (r - p + 1));

  int i = p;
  int j = q + 1;
  int k = 0;
  int l;

  /* as long as both lists have unexamined elements */
  /*  this loop keeps executing. */
  while ((i <= q) && (j <= r))
    {
      if (A[i] < A[j])
        {
	  B[k] = A[i];
	  i++;
        }
      else
        {
	  B[k] = A[j];
	  j++;
        }
      k++;
    }

  /* now only at most one list has unprocessed elements. */
  if (i <= q)
    {
      /* copy remaining elements from the first list */
      for (l = i; l <= q; l++)
        {
	  B[k] = A[l];
	  k++;
        }
    }
  else
    {
      /* copy remaining elements from the second list */
      for (l = j; l <= r; l++)
        {
	  B[k] = A[l];
	  k++;
        }
    }

  /* copy merged output from array B back to array A */
  k = 0;
  for (l = p; l <= r; l++)
    {
      A[l] = B[k];
      k++;
    }

  free(B);
}



void mergesort_mt(int *A, int n, int num_thread)
 {
    //num_thread = min(num_thread, MAX_THREADS
    if (num_thread > MAX_THREADS)
    {
      num_thread = MAX_THREADS;
    }
    //
    int chunk_size = 1 + ((n - 1) / num_thread);


    int beginning = 0;
    int ending = chunk_size - 1;

    // sort the chunks
    while (beginning < n)
    {
      mergesort_s(A, beginning, ending);
      beginning = ending + 1;
      ending = ending + chunk_size;

      //ending cannot be bigger then n
      if (ending >= n)
      {
        ending = n - 1;
      }
    }

    //join threads

    

    //now merge it
    beginning = 0;
    ending = chunk_size - 1;
    while (beginning < n)
    {
      merge_s(A, beginning, ending, ending + chunk_size);
      beginning = ending + chunk_size + 1;
      ending = ending + chunk_size + chunk_size;
      if (ending >= n)
      {
        ending = n - 1;
      }
    }


    //FREE MEMORY
  
 }

double getMilliSeconds()
{
  struct timeval now;
  gettimeofday(&now, (struct timezone *)0);
  return (double)now.tv_sec * 1000.0 + now.tv_usec / 1000.0;
}