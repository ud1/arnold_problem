package org.arnold;

import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.BitSet;
import java.util.List;
import java.util.stream.Collectors;

public class Generators
{
    private int[] generators;
    private PolygonStats polygonStats;
    private OMatrix oMatrix;

    public Generators(int[] generators)
    {
        this.generators = generators;
    }

    public Generators(String line)
    {
        this(Arrays.stream(line.split("\\s+")).mapToInt(Integer::parseInt).toArray());
    }

    public int getLinesNumber()
    {
        return Arrays.stream(generators).max().getAsInt() + 2;
    }

    public int getGeneratorsNumber()
    {
        return generators.length;
    }

    public int getGenerator(int i)
    {
        return generators[i];
    }

    public List<Polygon> computePolygons()
    {
        int numLines = getLinesNumber();
        int[] lines = new int[numLines];

        for (int i = 0; i < numLines; ++i)
        {
            lines[i] = i;
        }

        PolygonBuilder[] d = new PolygonBuilder[numLines + 1];
        for (int i = 0; i <= numLines; ++i)
        {
            PolygonBuilder b = new PolygonBuilder();
            d[i] = b;
            b.external = true;
            b.isBlack = i % 2 == 0;
            if (i - 1 > 0)
                b.lines.add(i - 1);
            if (i < numLines)
                b.lines.add(i);
        }

        List<PolygonBuilder> polygons = new ArrayList<>();

        int i = 0;
        for (int g : generators)
        {
            if (g < 0)
                throw new RuntimeException("Invalid configuration " + this);

            if (lines[g] > lines[g + 1])
                throw new RuntimeException("Invalid configuration " + this);

            d[g + 1].vertices.add(i);
            polygons.add(d[g + 1]);

            PolygonBuilder b = d[g + 2];
            b.vertices.add(i);
            b.lines.add(lines[g]);

            b = d[g];
            b.vertices.add(i);
            b.lines.add(lines[g + 1]);

            int temp = lines[g];
            lines[g] = lines[g + 1];
            lines[g + 1] = temp;

            b = new PolygonBuilder();
            b.external = false;
            b.isBlack = g % 2 == 1;
            b.lines.add(lines[g]);
            b.lines.add(lines[g + 1]);
            b.vertices.add(i);
            d[g + 1] = b;

            ++i;
        }

        for (PolygonBuilder b : d)
        {
            b.external = true;
            polygons.add(b);
        }

        List<Polygon> result = new ArrayList<>();
        for (PolygonBuilder b : polygons)
        {
            Polygon p = new Polygon(
                    b.lines.stream().mapToInt(Integer::intValue).toArray(),
                    b.vertices.stream().mapToInt(Integer::intValue).toArray(),
                    b.external,
                    b.isBlack
            );
            result.add(p);
        }

        return result;
    }

    public List<Integer> computeLastLinesOrder()
    {
        int numLines = getLinesNumber();
        List<Integer> lines = new ArrayList<>(numLines);

        for (int i = 0; i < numLines; ++i)
        {
            lines.add(i);
        }

        for (int g : generators)
        {
            if (g < 0)
                throw new RuntimeException("Invalid configuration " + this);

            if (lines.get(g) > lines.get(g + 1))
                throw new RuntimeException("Invalid configuration " + this);

            int temp = lines.get(g);
            lines.set(g, lines.get(g + 1));
            lines.set(g + 1, temp);
        }

        return lines;
    }

    @Override
    public String toString()
    {
        StringBuilder b = new StringBuilder();
        for (int i = 0; i < generators.length; ++i)
        {
            if (i > 0)
                b.append(" ");
            b.append(generators[i]);
        }

        return b.toString();
    }

    public static class PolygonBuilder
    {
        List<Integer> lines = new ArrayList<>();
        List<Integer> vertices = new ArrayList<>();
        boolean external;
        boolean isBlack;
    }

    public static List<Generators> load(File file) throws IOException
    {
        return Files.readAllLines(file.toPath()).stream()
                .filter(s -> !s.isEmpty())
                .map(Generators::new)
                .collect(Collectors.toList());
    }

    public String printMetroMap()
    {
        StringBuilder sb = new StringBuilder();

        int numLines = getLinesNumber();
        appendVerticalLines(sb, numLines);
        sb.append('\n');

        for (int g : generators)
        {
            appendVerticalLines(sb, g);
            if (g > 0)
                sb.append("  ");

            sb.append(" >< ");

            if (numLines - g - 2 > 0)
                sb.append("  ");

            appendVerticalLines(sb, numLines - g - 2);
            sb.append('\n');
        }

        appendVerticalLines(sb, numLines);

        return sb.toString();
    }

    public String printMetroMapCondensed()
    {
        int numLines = getLinesNumber();
        List<BitSet> gens = new ArrayList<>(numLines);

        for (int g : generators)
        {
            BitSet bs = new BitSet(numLines - 1);
            bs.set(g);
            gens.add(bs);
        }

        for (int i = 0; i < gens.size(); ++i)
        {
            BitSet bs = gens.get(i);
            if (bs == null)
                continue;

            int lastJ = i;
            for (int j = i; j --> 0;)
            {
                BitSet bs2 = gens.get(j);
                if (bs2 == null)
                    continue;

                if (!isCompatible(bs, bs2))
                {
                    break;
                }

                lastJ = j;
            }

            if (lastJ != i)
            {
                BitSet bs2 = gens.get(lastJ);
                bs2.or(bs);
                gens.set(i, null);
            }
        }

        StringBuilder sb = new StringBuilder();
        appendVerticalLines(sb, numLines);
        sb.append('\n');

        for (BitSet bs : gens)
        {
            if (bs == null)
                continue;

            for (int i = 0; i < numLines * 2 - 1; ++i)
            {
                if (i % 2 == 0)
                {
                    int l = i / 2;

                    boolean cross = (l > 0) && bs.get(l - 1) || bs.get(l);

                    if (cross)
                        sb.append(' ');
                    else
                        sb.append('|');
                }
                else
                {
                    int l = i / 2;

                    if (bs.get(l))
                    {
                        sb.append("><");
                    }
                    else
                    {
                        sb.append("  ");
                    }
                }
            }
            sb.append('\n');
        }

        appendVerticalLines(sb, numLines);

        return sb.toString();
    }

    public PolygonStats getPolygonStats() {
        if (polygonStats == null)
            polygonStats = new PolygonStats(computePolygons());
        return polygonStats;
    }

    public OMatrix getOMatrix() {
        if (oMatrix == null)
            oMatrix = new OMatrix(this);

        return oMatrix;
    }

    public boolean equalsPlain(Generators oth) {
        if (!getPolygonStats().equals(oth.getPolygonStats()))
            return false;

        return getOMatrix().equalsPlain(oth.getOMatrix());
    }

    public boolean equalsSphere(Generators oth) {
        if (!getPolygonStats().getTotalPolygons().equals(oth.getPolygonStats().getTotalPolygons()))
            return false;

        return getOMatrix().equalsSphere(oth.getOMatrix());
    }

    private static boolean isCompatible(BitSet s1, BitSet s2)
    {
        for (int i = 0; i < s1.length(); ++i)
        {
            if (s1.get(i))
            {
                if (i - 1 >= 0 && s2.get(i - 1))
                    return false;

                if (i + 1 < s2.length() && s2.get(i + 1))
                    return false;
            }
        }

        return true;
    }

    private static void appendVerticalLines(StringBuilder sb, int count)
    {
        for (int i = 0; i < count; ++i)
        {
            if (i > 0)
            {
                sb.append(' ');
                sb.append(' ');
            }
            sb.append('|');
        }
    }
}
