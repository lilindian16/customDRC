import os
import binascii


class FileConverter:
    def __init__(self, current_path, filename, output_filename):
        self.current_path = current_path
        self.filename = filename
        self.output_filename = current_path + \
            "/customDRC_header_files/" + output_filename

        print(f"Path: {self.current_path}")
        print(f"Filename: {self.filename}")
        print(f"Output filename: {self.output_filename}")

    def process_file(self, ):
        filepath = self.current_path + "/" + self.filename
        file = open(filepath, "rb")
        file_contents = file.read()

        output_file_path = self.output_filename
        output_file = open(output_file_path, "w")

        output_file.write("#pragma once\n")

        if ".html" in self.filename:
            output_file.write("const char custom_html[] = {")

        elif ".css" in self.filename:
            output_file.write("const char custom_css[] = {")

        elif ".js" in self.filename:
            output_file.write("const char custom_js[] = {")

        for character in file_contents[:-1]:
            output_file.write(hex(character) + ",")
            print(hex(character), end=",")
        print(hex(file_contents[-1]))
        output_file.write(hex(file_contents[-1]))
        output_file.write("};\n")
        if (file_contents[-1] != 0xa):
            output_file.write('\n')


cwd = os.getcwd()
html_file = FileConverter(cwd, "index.html", "CustomDRChtml.h")
css_file = FileConverter(cwd, "pico.min.css", "CustomDRCcss.h")
js_file = FileConverter(cwd, "index.js", "CustomDRCjs.h")

html_file.process_file()
css_file.process_file()
js_file.process_file()
