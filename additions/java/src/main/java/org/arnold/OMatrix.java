package org.arnold;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.TreeMap;

public class OMatrix
{
    private final ArrayList<ArrayList<Integer>> lineIntersections;
    private final int numLines;
    private final int numVertices;
    private final Map<Integer, Integer> parallels = new TreeMap<>();
    private final int hash;

    private OMatrix(ArrayList<ArrayList<Integer>> lineIntersections, int numLines, int numVertices)
    {
        this.lineIntersections = lineIntersections;
        this.numLines = numLines;
        this.numVertices = numVertices;
        this.hash = computeHash();
    }

    private int computeHash()
    {
        int hash = 1;
        for (ArrayList<Integer> r : lineIntersections)
        {
            for (Integer v : r)
            {
                hash = hash * 106039 + v;
            }
        }

        return hash;
    }

    public OMatrix(Generators generators)
    {
        numLines = generators.getLinesNumber();
        numVertices = generators.getGeneratorsNumber();
        lineIntersections = new ArrayList<>(numLines);
        int[] lines = new int[numLines];

        for (int i = 0; i < numLines; ++i)
        {
            lineIntersections.add(new ArrayList<>());
            lines[i] = i;
        }

        for (int i = 0; i < generators.getGeneratorsNumber(); ++i)
        {
            int gen = generators.getGenerator(i);

            int first = lines[gen];
            int second = lines[gen + 1];

            lineIntersections.get(first).add(second);
            lineIntersections.get(second).add(first);

            lines[gen] = second;
            lines[gen + 1] = first;
        }

        for (int i = 0; i < numLines - 1; ++i)
        {
            if (lines[i] < lines[i + 1])
            {
                parallels.put(lines[i], lines[i + 1]);
                parallels.put(lines[i + 1], lines[i]);
            }
        }

        this.hash = computeHash();
    }

    public OMatrix extractSubmatrix(int[] lines)
    {
        Arrays.sort(lines);
        int[] lineInds = new int[numLines];

        {
            int j = 0;
            for (int i = 0; i < numLines; ++i)
            {
                if (j < lines.length && lines[j] == i)
                {
                    lineInds[i] = j;
                    ++j;
                }
                else
                {
                    lineInds[i] = -1;
                }
            }
        }

        int newNumVertices = 0;
        ArrayList<ArrayList<Integer>> newLineIntersections = new ArrayList<>(lines.length);
        for (int i = 0; i < numLines; ++i)
        {
            if (lineInds[i] >= 0)
            {
                ArrayList<Integer> oldRow = lineIntersections.get(i);
                ArrayList<Integer> row = new ArrayList<>(lines.length - 1);
                newLineIntersections.add(row);

                for (int j = 0; j < oldRow.size(); ++j)
                {
                    int newInd = lineInds[oldRow.get(j)];
                    if (newInd >= 0)
                    {
                        row.add(newInd);
                        newNumVertices++;
                    }
                }
            }
        }

        return new OMatrix(newLineIntersections, lines.length, newNumVertices/2);
    }

    public OMatrix sphereRotation(int val)
    {
        if (numLines % 2 == 0 || !parallels.isEmpty())
            throw new RuntimeException("Only odd configurations supported");

        Map<Integer, Integer> reord = new HashMap<>();
        {
            ArrayList<Integer> line = lineIntersections.get(val);
            for (int i = 0; i < numLines - 1; ++i)
            {
                reord.put(line.get(i), i);
            }

            reord.put(numLines, numLines - 1);
        }

        ArrayList<ArrayList<Integer>> temp = new ArrayList<>(numLines + 1);
        for (int i = 0; i < numLines; ++i)
        {
            ArrayList<Integer> line = new ArrayList<>(numLines);
            line.addAll(lineIntersections.get(i));
            line.add(numLines);
            temp.add(line);
        }

        {
            ArrayList<Integer> line = new ArrayList<>(numLines);
            for (int i = 0; i < numLines; ++i)
                line.add(i);
            temp.add(line);
        }

        ArrayList<ArrayList<Integer>> newLineIntersections = new ArrayList<>(numLines);
        for (int i = 0; i < numLines; ++i)
        {
            ArrayList<Integer> newLine = new ArrayList<>();
            newLineIntersections.add(newLine);

            int v = temp.get(val).get(i);
            ArrayList<Integer> tempLine = temp.get(v);
            if (v < val)
            {
                int valPos = -1;
                for (int j = 0; j < numLines; ++j)
                {
                    if (tempLine.get(j).equals(val))
                    {
                        valPos = j;
                        break;
                    }
                }

                for (int j = valPos; j --> 0;)
                {
                    newLine.add(reord.get(tempLine.get(j)));
                }

                for (int j = numLines; j --> valPos + 1;)
                {
                    newLine.add(reord.get(tempLine.get(j)));
                }
            }
            else
            {
                int valPos = -1;
                for (int j = 0; j < numLines; ++j)
                {
                    if (tempLine.get(j).equals(val))
                    {
                        valPos = j;
                        break;
                    }
                }

                for (int j = valPos + 1; j < numLines; ++j)
                {
                    newLine.add(reord.get(tempLine.get(j)));
                }

                for (int j = 0; j < valPos; ++j)
                {
                    newLine.add(reord.get(tempLine.get(j)));
                }
            }
        }

        return new OMatrix(newLineIntersections, numLines, numVertices);
    }

    public OMatrix reflect()
    {
        ArrayList<ArrayList<Integer>> lineIntersections = new ArrayList<>();

        for (int i = numLines; i --> 0;)
        {
            ArrayList<Integer> line = new ArrayList<>();
            for (int l : this.lineIntersections.get(i))
            {
                line.add(numLines - 1 - l);
            }

            lineIntersections.add(line);
        }

        OMatrix result = new OMatrix(lineIntersections, numLines, numVertices);
        this.parallels.forEach((k, v) -> {
            result.parallels.put(numLines - 1 - k, numLines - 1 - v);
        });
        return result;
    }

    public boolean canRotate(int rotation)
    {
        if (rotation < 0 || rotation >= 2*numLines)
            return false;

        int rotated = norm(rotation);
        return parallels.getOrDefault(rotated, rotated) >= rotated;
    }

    public Optional<OMatrix> rotate(int rotation)
    {
        rotation %= (2*numLines);
        if (rotation == 0)
            return Optional.of(this);

        if (!canRotate(rotation))
            return Optional.empty();

        ArrayList<ArrayList<Integer>> lineIntersections = new ArrayList<>();

        for (int i = 0; i < numLines; ++i)
        {
            ArrayList<Integer> line = new ArrayList<>();
            int len = getLineLen(rotation, i);
            for (int j = 0; j < len; ++j)
            {
                line.add(getVal(rotation, i, j));
            }
            lineIntersections.add(line);
        }

        OMatrix result = new OMatrix(lineIntersections, numLines, numVertices);
        int finalRotation = rotation;
        this.parallels.forEach((k, v) -> {
            result.parallels.put(rotateLineNum(finalRotation, k), rotateLineNum(finalRotation, v));
        });

        return Optional.of(result);
    }

    public boolean checkCmSymmetry(int m)
    {
        if (2 * numLines % m != 0)
            return false;

        int rotation = 2 * numLines / m;

        Optional<OMatrix> r = rotate(rotation);
        if (r.isPresent())
            return r.get().equals(this);

        return false;
    }

    public Optional<Integer> findCmSymmetry()
    {
        for (int i = numLines + 1; i --> 2;)
        {
            if (checkCmSymmetry(i))
                return Optional.of(i);
        }
        return Optional.empty();
    }

    public boolean checkD2Symmetry()
    {
        OMatrix oth = reflect();
        if (this.equals(oth))
            return true;

        for (int i = 0; i < 2 * numLines; ++i)
        {
            Optional<OMatrix> r = rotate(i);
            if (r.isPresent() && r.get().equals(oth))
                return true;
        }

        return false;
    }

    public int getVal(int rotation, int i, int j)
    {
        int v;
        int l = norm2(i + rotation);
        if (l < numLines)
        {
            v = lineIntersections.get(l).get(j);
        }
        else
        {
            l = norm(l);
            l = parallels.getOrDefault(l, l);
            ArrayList<Integer> line = lineIntersections.get(l);
            v = line.get(line.size() - 1 - j);
        }

        if (rotation < numLines && v >= rotation || rotation >= numLines && v < norm(rotation))
        {
            return norm(v - rotation);
        }

        v = parallels.getOrDefault(v, v);
        return norm(v - rotation);
    }

    private int rotateLineNum(int rotation, int l)
    {
        if (l >= rotation)
            return l - rotation;

        return norm(l - rotation);
    }

    private int getLineLen(int rotation, int i)
    {
        int l = i + rotation;
        if (l < numLines)
        {
            return lineIntersections.get(l).size();
        }
        else
        {
            l = norm(l);
            l = parallels.getOrDefault(l, l);
            return lineIntersections.get(l).size();
        }
    }

    private int norm(int l)
    {
        return (l + numLines*2) % numLines;
    }

    private int norm2(int l)
    {
        return (l + numLines*2) % (2*numLines);
    }

    public Generators getGenerators()
    {
        int[] lines = new int[numLines];
        int[] pos = new int[numLines];
        for (int i = 0; i < numLines; ++i)
        {
            lines[i] = i;
            pos[i] = 0;
        }

        int[] generators = new int[numVertices];

        for (int i = 0; i < numVertices; ++i)
        {
            boolean found = false;
            for (int gen = 0; gen < numLines - 1; ++gen)
            {
                int left = lines[gen];
                int right = lines[gen + 1];
                if (pos[left] < lineIntersections.get(left).size() && pos[right] < lineIntersections.get(right).size()
                        && lineIntersections.get(left).get(pos[left]).equals(right) && lineIntersections.get(right).get(pos[right]).equals(left))
                {
                    pos[left]++;
                    pos[right]++;
                    lines[gen] = right;
                    lines[gen + 1] = left;
                    generators[i] = gen;
                    found = true;
                    break;
                }
            }

            if (!found)
                throw new RuntimeException("Invalid OMatrix");
        }

        return new Generators(generators);
    }

    @Override
    public String toString()
    {
        final StringBuilder sb = new StringBuilder();

        int maxNumLen = (int) Math.ceil(Math.log10(numLines));
        for (int i = 0; i < numLines; ++i)
        {
            ArrayList<Integer> l = lineIntersections.get(i);

            append(sb, i, maxNumLen);
            sb.append(" || ");
            append(sb, parallels.getOrDefault(i, i), maxNumLen);
            sb.append(") ");

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

    @Override
    public boolean equals(Object obj)
    {
        if (!(obj instanceof OMatrix))
            return false;

        OMatrix oth = (OMatrix) obj;

        return numLines == oth.numLines &&
                numVertices == oth.numVertices &&
                lineIntersections.equals(oth.lineIntersections) &&
                parallels.equals(oth.parallels);
    }

    public boolean equalsPlain(OMatrix oth)
    {
        for (int i = 0; i < 2 * numLines; ++i)
        {
            Optional<OMatrix> r = rotate(i);
            if (r.isPresent() && r.get().equals(oth))
                return true;
        }

        OMatrix reflected = reflect();
        for (int i = 0; i < 2 * numLines; ++i)
        {
            Optional<OMatrix> r = reflected.rotate(i);
            if (r.isPresent() && r.get().equals(oth))
                return true;
        }

        return false;
    }

    public boolean equalsSphere(OMatrix oth)
    {
        for (int i = 0; i < numLines; ++i)
        {
            OMatrix m = oth.sphereRotation(i);
            if (m.equalsPlain(this))
                return true;
        }

        return false;
    }

    public int getNumLines()
    {
        return numLines;
    }

    public int getNumVertices()
    {
        return numVertices;
    }

    public List<Integer> getLine(int l)
    {
        return Collections.unmodifiableList(lineIntersections.get(l));
    }

    @Override
    public int hashCode()
    {
        return hash;
    }
}
