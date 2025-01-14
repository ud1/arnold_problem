package org.arnold;

import java.util.TreeMap;

public class CounterMap extends TreeMap<Integer, Integer>
{
    public void inc(int key)
    {
        compute(key, (k, v) -> {
            if (v == null)
                return 1;
            return v + 1;
        });
    }
}
