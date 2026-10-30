#ifndef SUM_LIB_H
#define SUM_LIB_H

struct SumArgs {
  int *array;
  int begin;
  int end;
};

int Sum(const struct SumArgs *args);

#endif // SUM_LIB_H