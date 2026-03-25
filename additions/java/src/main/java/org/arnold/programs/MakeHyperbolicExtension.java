package org.arnold.programs;

import org.arnold.Generators;
import org.arnold.OMatrix;
import org.arnold.PolygonStats;

public class MakeHyperbolicExtension {
    public static void main(String[] args) {
        Generators g = new Generators("1 3 5 7 6 5 4 3 2 1 0 1 3 5 4 3 2 1 3 5 7 6 5 4 3 2 1 3 5 7 6 5 4 3 5 7");
        OMatrix hExt = g.getOMatrix().hyperbolicExtension();
        Generators hGens = hExt.getGenerators();
        System.out.println(hGens);
        System.out.println(new PolygonStats(hGens.computePolygons()));
    }
}
