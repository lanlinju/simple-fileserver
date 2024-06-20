package com.example.simplefileserver

import org.junit.jupiter.api.Test
import org.junit.jupiter.api.Assertions.*
import org.springframework.boot.test.context.SpringBootTest
import java.net.URLConnection

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

    @Test
    fun testFileMimeType() {
        val input = "1.mp4"
        val expected = "video/mp4"
        val actual = URLConnection.guessContentTypeFromName(input)

        assertEquals(expected, actual)

        val input2 = "1.png"
        val expected2 = "image/png"
        val actual2 = URLConnection.guessContentTypeFromName(input2)

        assertEquals(expected2, actual2)

        val input3 = "1."
        val expected3 = null
        val actual3 = URLConnection.guessContentTypeFromName(input3)

        assertEquals(expected3, actual3)

    }
}
