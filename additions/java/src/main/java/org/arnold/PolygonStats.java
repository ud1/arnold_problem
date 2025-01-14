package org.arnold;

import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.Objects;

public class PolygonStats
{
    private final CounterMap externalPolygons = new CounterMap();
    private final CounterMap internalPolygons = new CounterMap();
    private final CounterMap sumPolygons = new CounterMap();
    private final int difference;

    public PolygonStats(List<Polygon> polygons)
    {
        int difference = 0;
        for (Polygon polygon : polygons)
        {
            if (polygon.isExternal())
            {
                externalPolygons.inc(polygon.getVertices().length);
                sumPolygons.inc(polygon.getVertices().length + 2);
            }
            else
            {
                internalPolygons.inc(polygon.getVertices().length);
                sumPolygons.inc(polygon.getVertices().length);
            }

            if (polygon.isBlack())
                ++difference;
            else
                --difference;
        }

        this.difference = Math.abs(difference);
    }

    @Override
    public String toString()
    {
        final StringBuilder sb = new StringBuilder("PolygonStats{");
        sb.append("externalPolygons=").append(externalPolygons);
        sb.append(", internalPolygons=").append(internalPolygons);
        sb.append(", sumPolygons=").append(sumPolygons);
        sb.append(", difference=").append(difference);
        sb.append('}');
        return sb.toString();
    }

    @Override
    public boolean equals(Object o) {
        if (!(o instanceof PolygonStats))
            return false;
        PolygonStats that = (PolygonStats) o;
        return difference == that.difference &&
                Objects.equals(externalPolygons, that.externalPolygons) &&
                Objects.equals(internalPolygons, that.internalPolygons) &&
                Objects.equals(sumPolygons, that.sumPolygons);
    }

    @Override
    public int hashCode() {
        return Objects.hash(externalPolygons, internalPolygons, sumPolygons, difference);
    }

    public Map<Integer, Integer> getTotalPolygons() {
        return Collections.unmodifiableMap(sumPolygons);
    }
}
