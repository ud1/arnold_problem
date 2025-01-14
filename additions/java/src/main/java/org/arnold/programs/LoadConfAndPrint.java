package org.arnold.programs;

import org.arnold.Generators;
import org.arnold.PolygonStats;

import java.io.File;
import java.util.List;

public class LoadConfAndPrint {
    public static void main(String[] args) throws Exception {
        List<Generators> generators = Generators.load(new File("/media/arnold/generators/21-0.txt"));

        Generators g = generators.get(0);
        System.out.println(g.printMetroMapCondensed());
        System.out.println(new PolygonStats(g.computePolygons()));

        System.out.println("D2: " + g.getOMatrix().checkD2Symmetry());
        System.out.println("CM: " + g.getOMatrix().findCmSymmetry());

        for (int i = 0; i < 21; i+=2) {
            Generators newGens = g.getOMatrix().rotate(i).get().getGenerators();
            System.out.println(newGens.printMetroMapCondensed());
            System.out.println();
        }
    }
}
