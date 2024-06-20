package com.example.simplefileserver

import org.junit.jupiter.api.Test
import org.junit.jupiter.api.Assertions.*
import org.springframework.boot.test.context.SpringBootTest

@SpringBootTest
class SimpleFileserverApplicationTests {

    @Test
    fun contextLoads() {
    }

    @Test
    fun test() {
        val input = "bytes=0-499"
        val expected = "0"
        val expected2 = "499"

        val ranges = input.removePrefix("bytes=").split("-")
        val actual = ranges[0]
        val actual2 = ranges[1]

        assertEquals(expected, actual)
        assertEquals(expected2, actual2)

        val input2 = "bytes=0-"
        val expected3 = 1234L
        val ranges2 = input2.removePrefix("bytes=").split("-")
        val actual3 = if (ranges2.size > 1 && ranges2[1].isNotEmpty()) ranges2[1].toLong() else 1234
        assertEquals(2, ranges2.size) // 0- to ["0", ""]
        assertEquals(true, ranges2[1].isEmpty())

        assertEquals(expected3, actual3)

    }

    @Test
    fun testIntRange() {
        val input = 1..8
        val expected = 1
        val expected2 = 8

        assertEquals(expected, input.first)
        assertEquals(expected, input.start)
        assertEquals(expected2, input.last)
        assertEquals(expected2, input.endInclusive)
    }
}
