


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



typedef struct {
    int *A;
    int low;
    int high;
} SortArgs;

typedef struct {
    int *A;
    int low;
    int mid;
    int high;
} MergeArgs;

void *mergesort_thread(void *arg)
{
    SortArgs *args = (SortArgs *)arg;
    mergesort_s(args->A, args->low, args->high);
    free(arg);
    return NULL;
}

void *merge_thread(void *arg)
{
    MergeArgs *args = (MergeArgs *)arg;
    merge_s(args->A, args->low, args->mid, args->high);
    free(arg);
    return NULL;
}

void mergesort_mt(int *A, int n, int num_thread)
{
    if (num_thread > MAX_THREADS)
    {
        num_thread = MAX_THREADS;
    }

    int num_chunks = num_thread;
    int chunk_size = (n + num_chunks -1) / num_chunks;

    // Create an array to store thread IDs
    pthread_t threads[num_chunks];

    // Sorting phase: sort each chunk in parallel
    for (int i = 0; i < num_chunks; i++)
    {
        int low = i * chunk_size;
        int high = (i + 1) * chunk_size - 1;
        if (high >= n)
        {
            high = n - 1;
        }
        if (low >= n) {
            break;
        }

        SortArgs *args = malloc(sizeof(SortArgs));
        args->A = A;
        args->low = low;
        args->high = high;

        pthread_create(&threads[i], NULL, mergesort_thread, args);
    }

    // Wait for all sorting threads to finish
    for (int i = 0; i < num_chunks; i++)
    {
        pthread_join(threads[i], NULL);
    }

    // Merging phase
    int curr_num_chunks = num_chunks;
    int curr_chunk_size = chunk_size;

    while (curr_num_chunks > 1)
    {
        int num_pairs = curr_num_chunks / 2;
        int merge_threads = 0;

        for (int i = 0; i < num_pairs; i++)
        {
            int low = 2 * i * curr_chunk_size;
            int mid = low + curr_chunk_size - 1;
            int high = (2 * i + 2) * curr_chunk_size - 1;

            if (mid >= n)
            {
                mid = n - 1;
            }
            if (high >= n)
            {
                high = n - 1;
            }
            if (low >= n || mid >= high)
            {
                // No valid data to merge
                continue;
            }

            MergeArgs *args = malloc(sizeof(MergeArgs));
            args->A = A;
            args->low = low;
            args->mid = mid;
            args->high = high;

            pthread_create(&threads[merge_threads], NULL, merge_thread, args);
            merge_threads++;
        }

        // Wait for merge threads to finish
        for (int i = 0; i < merge_threads; i++)
        {
            pthread_join(threads[i], NULL);
        }

        // Handle the case of an odd number of chunks
        curr_num_chunks = (curr_num_chunks + 1) / 2;
        curr_chunk_size *= 2;
    }
}

double getMilliSeconds()
{
  struct timeval now;
  gettimeofday(&now, (struct timezone *)0);
  return (double)now.tv_sec * 1000.0 + now.tv_usec / 1000.0;
}