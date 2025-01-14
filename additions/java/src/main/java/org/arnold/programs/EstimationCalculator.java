package org.arnold.programs;

public class EstimationCalculator
{
    public static void main(String[] args) {
        int N = 53;

        int b4 = (N*N + N) % 3;
        int A = (N*N + N - 6 - 4*b4)/6;

        System.out.println(String.format("A=%d\tb4=%d", A, b4));
    }
}
