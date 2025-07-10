#pragma once

// Тестирование оптимизаций RCKangaroo
// Проверяет корректность векторизированных операций

#include "defs.h"

#ifdef __cplusplus
extern "C" {
#endif

// Функции для тестирования на CPU
bool test_vectorized_operations();
bool test_optimized_copy();
bool test_conditional_neg();

// Вспомогательные функции
bool compare_u64_arrays(const u64* a, const u64* b, int count);
void print_u64_array(const u64* arr, int count, const char* name);

#ifdef __cplusplus
}
#endif