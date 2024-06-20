package com.example.simplefileserver

import org.springframework.boot.autoconfigure.SpringBootApplication
import org.springframework.boot.runApplication

@SpringBootApplication
class SimpleFileserverApplication

fun main(args: Array<String>) {
	runApplication<SimpleFileserverApplication>(*args)
}
