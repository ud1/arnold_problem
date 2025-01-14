package org.arnold;

import java.util.Map;

public class ConfigurationStats
{
    private int numParallels;
    private int numLines;
    private Map<Integer, Integer> whiteAreasInternal;
    private Map<Integer, Integer> whiteAreasExternal;
    private Map<Integer, Integer> blackAreasInternal;
    private Map<Integer, Integer> blackAreasExternal;

    public ConfigurationStats(int numParallels, int numLines, Map<Integer, Integer> whiteAreasInternal, Map<Integer, Integer> whiteAreasExternal, Map<Integer, Integer> blackAreasInternal, Map<Integer, Integer> blackAreasExternal)
    {
        this.numParallels = numParallels;
        this.numLines = numLines;
        this.whiteAreasInternal = whiteAreasInternal;
        this.whiteAreasExternal = whiteAreasExternal;
        this.blackAreasInternal = blackAreasInternal;
        this.blackAreasExternal = blackAreasExternal;
    }
}
