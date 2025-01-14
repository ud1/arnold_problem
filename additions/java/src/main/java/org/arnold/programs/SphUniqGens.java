package org.arnold.programs;

import org.arnold.Generators;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public class SphUniqGens {
    public static void main(String[] args) throws IOException
    {
        List<Generators> generators = Generators.load(new File("/media/arnold/generators/23-0.txt"));
        List<Generators> generators2 = new ArrayList<>();

        int i = 0;
        for (Generators g : generators)
        {
            ++i;
            System.out.println("P " + i);
            boolean duplicate = false;
            for (Generators g2 : generators2)
            {
                if (g2.equalsSphere(g))
                {
                    duplicate = true;
                    break;
                }
            }

            if (!duplicate)
            {
                generators2.add(g);
            }
        }

        for (Generators g : generators2)
        {
            System.out.println(g);
        }

        System.out.println(generators.size());
        System.out.println(generators2.size());
    }
}
