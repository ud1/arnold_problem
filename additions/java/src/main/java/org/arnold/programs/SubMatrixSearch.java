package org.arnold.programs;

import org.arnold.CombinationsGosper;
import org.arnold.Generators;
import org.arnold.OMatrix;

import java.util.HashSet;

public class SubMatrixSearch
{
    public static void main(String[] args)
    {
        // Нераспрямляемые комбинации из 8 линий
        HashSet<OMatrix> na8 = new HashSet<>();
        {
            fillAllCombinations(na8, new Generators("1 0 2 1 0 3 4 3 2 1 6 5 4 3 2 1 0 4 3 5 6 5 4 3 2 1 2 3").getOMatrix());
            fillAllCombinations(na8, new Generators("1 3 2 4 3 2 1 0 1 2 3 4 3 5 4 6 5 4 3 2 1 0 2 3 4 3 2 1").getOMatrix());
            fillAllCombinations(na8, new Generators("1 0 2 3 2 1 0 4 3 2 3 4 5 4 3 2 1 2 3 6 5 4 3 2 1 0 1 2").getOMatrix());
        }

        Generators g = new Generators("0 2 4 6 5 4 3 2 1 2 4 8 10 12 11 10 9 8 7 6 5 4 3 2 4 8 10 14 16 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 4 8 10 9 8 7 6 8 10 11 12 14 13 12 14 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 6 8 10 12 11 10 9 8 7 6 5 4 6 8 10 12 14 13 12 11 10 12 16 15 14 13 12 14 17 16 15 14 16 18 17 16 15 14 13 12 11 10 9 8 7 6 8 10 12 16 18 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 2 4 6 5 4 3 2 4 6 5 7 6 8 10 12 11 10 9 8 7 6 8 10 9 8 10 11 12 14 13 12 14 15 14 13 12 11 10 9 8 7 6 5 4 6 8 12 14 16 15 14 13 12 11 10 12 14 18 17 16 18");

        CombinationsGosper.combinations(g.getLinesNumber(), 8).anyMatch(lines -> {
            OMatrix sub8 = g.getOMatrix().extractSubmatrix(lines);
            if (na8.contains(sub8))
            {
                System.out.println("FOUND!!!");
                return true;
            }
            return false;
        });
    }

    private static void fillAllCombinations(HashSet<OMatrix> combinations, OMatrix o)
    {
        for (int i = 0; i < o.getNumLines() * 2; ++i)
        {
            o.rotate(i).ifPresent(combinations::add);
        }

        o = o.reflect();
        for (int i = 0; i < o.getNumLines() * 2; ++i)
        {
            o.rotate(i).ifPresent(combinations::add);
        }
    }
}
