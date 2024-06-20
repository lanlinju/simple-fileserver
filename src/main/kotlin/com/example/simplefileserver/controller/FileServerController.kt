package com.example.simplefileserver.controller

import jakarta.servlet.http.HttpServletRequest
import jakarta.servlet.http.HttpServletResponse
import org.springframework.web.bind.annotation.RequestMapping
import org.springframework.web.bind.annotation.RestController

@RestController
@RequestMapping("/file")
class FileServerController {

    @RequestMapping("/*")
    fun handleFileRequest(request: HttpServletRequest, response: HttpServletResponse) {
        response.writer.write("Hello world!")
    }
}