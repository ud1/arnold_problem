package org.arnold;

import java.util.stream.Stream;

public class CombinationsGosper
{
    public static Stream<int[]> combinations(int n, int k)
    {
        if (n < 0 || n > 63) {
            throw new IllegalArgumentException("n must be in [0, 63]");
        }
        if (k < 0 || k > n) {
            throw new IllegalArgumentException("k must be in [0, n]");
        }
        if (k == 0) {
            return Stream.of(new int[0]);
        }

        long firstMask = (1L << k) - 1;   // k младших битов = 1
        long limit     = 1L << n;         // как только маска >= 2^n — вылезли за диапазон

        return Stream.iterate(
                        firstMask,
                        mask -> mask < limit,
                        CombinationsGosper::nextMaskSameBitCount
                )
                .map(mask -> maskToCombination(mask, k));
    }

    // Gosper’s hack: следующая маска с тем же числом единичных битов
    private static long nextMaskSameBitCount(long x)
    {
        long c = x & -x;
        long r = x + c;
        return (((r ^ x) >>> 2) / c) | r;
    }

    // Преобразуем битовую маску в массив индексов установленных битов
    private static int[] maskToCombination(long mask, int k)
    {
        int[] res = new int[k];
        int idx = 0;
        while (mask != 0)
        {
            int bit = Long.numberOfTrailingZeros(mask); // позиция младшего установленного бита
            res[idx++] = bit;
            mask &= mask - 1; // снимаем младший установленный бит
        }
        return res;
    }
}
