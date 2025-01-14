package org.arnold;

import java.util.Arrays;

public class Polygon
{
    private int[] lines;
    private int[] vertices;
    private boolean external;
    private boolean isBlack;

    public Polygon(int[] lines, int[] vertices, boolean external, boolean isBlack)
    {
        this.lines = lines;
        this.vertices = vertices;
        this.external = external;
        this.isBlack = isBlack;
    }

    public int[] getLines()
    {
        return lines;
    }

    public int[] getVertices()
    {
        return vertices;
    }

    public boolean isExternal()
    {
        return external;
    }

    public boolean isBlack()
    {
        return isBlack;
    }

    @Override
    public String toString()
    {
        final StringBuilder sb = new StringBuilder("Polygon{");
        sb.append("lines=").append(Arrays.toString(lines));
        sb.append(", vertices=").append(Arrays.toString(vertices));
        sb.append(", external=").append(external);
        sb.append(", isBlack=").append(isBlack);
        sb.append('}');
        return sb.toString();
    }
}
