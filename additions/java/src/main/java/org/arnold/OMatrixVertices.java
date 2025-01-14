package org.arnold;

import java.util.ArrayList;

public class OMatrixVertices
{
    private final ArrayList<ArrayList<Integer>> lineVertices;
    private final int numLines;
    private final int numVertices;

    public OMatrixVertices(Generators generators)
    {
        numLines = generators.getLinesNumber();
        numVertices = generators.getGeneratorsNumber();
        lineVertices = new ArrayList<>(numLines);
        int[] lines = new int[numLines];

        for (int i = 0; i < numLines; ++i)
        {
            lineVertices.add(new ArrayList<>());
            lines[i] = i;
        }

        for (int i = 0; i < generators.getGeneratorsNumber(); ++i)
        {
            int gen = generators.getGenerator(i);

            int first = lines[gen];
            int second = lines[gen + 1];

            lineVertices.get(first).add(i);
            lineVertices.get(second).add(i);

            lines[gen] = second;
            lines[gen + 1] = first;
        }
    }

    @Override
    public String toString()
    {
        final StringBuilder sb = new StringBuilder();

        int maxNumLen = (int) Math.ceil(Math.log10(numVertices));
        int maxNumLinesLen = (int) Math.ceil(Math.log10(numLines));

        for (int i = 0; i < numLines; ++i)
        {
            ArrayList<Integer> l = lineVertices.get(i);

            append(sb, i, maxNumLinesLen);
            sb.append('|');

            for (int j = 0; j < l.size(); ++j)
            {
                int v = l.get(j);
                append(sb, v, maxNumLen);

                if (j + 1 < l.size())
                {
                    sb.append(' ');
                }
            }
            sb.append('\n');
        }

        return sb.toString();
    }

    private void append(StringBuilder sb, int v, int numWidth)
    {
        String n = String.valueOf(v);

        for (int i = 0; i < (numWidth - n.length()); ++i)
        {
            sb.append(' ');
        }

        sb.append(n);
    }
}
