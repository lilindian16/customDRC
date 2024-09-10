import os
import datetime


def get_current_time_header() -> str:
    # using now() to get current time
    current_time = datetime.datetime.now()
    return_message = f"/* Compile Time: {current_time.day}/{current_time.month}/{
        current_time.year} | {current_time.hour}:{current_time.minute}:{current_time.second} */ \n "
    return return_message


class FileConverter:
    def __init__(self, current_path, filename, output_filename):
        self.current_path = current_path
        self.filename = filename
        self.input_file_path = self.current_path + "/" + self.filename
        self.output_directory = self.current_path + "/CDRC_headers/"

        print(f"Path: {self.current_path}")
        print(f"Filename: {self.filename}")
        print(f"Input file path: {self.input_file_path}")
        print(f"Output file directory: {self.output_directory}")

    def compress_file(self, ):
        self.output_file = None
        if ".html" in self.filename:
            self.output_file_path = self.output_directory + "CustomDRChtml.h"
            self.output_file = open(self.output_file_path, 'w')
            self.output_file.write(get_current_time_header())
            self.output_file.write(
                "#pragma once\nconst char custom_html[] = {")
            self.output_file.close()
        elif ".css" in self.filename:
            self.output_file_path = self.output_directory + "CustomDRCcss.h"
            self.output_file = open(self.output_file_path, 'w')
            self.output_file.write(get_current_time_header())
            self.output_file.write("#pragma once\nconst char custom_css[] = {")
            self.output_file.close()
        elif ".js" in self.filename:
            self.output_file_path = self.output_directory + "CustomDRCjs.h"
            self.output_file = open(self.output_file_path, 'w')
            self.output_file.write(get_current_time_header())
            self.output_file.write("#pragma once\nconst char custom_js[] = {")
            self.output_file.close()

        self.output_file = open(self.output_file_path, 'a')
        raw_file = open(self.input_file_path, 'rb')
        raw_file_bytes = raw_file.read()
        for char in raw_file_bytes[:-1]:
            self.output_file.write(hex(char) + ",")
        self.output_file.write(hex(raw_file_bytes[-1]))
        self.output_file.write("," + hex(raw_file_bytes[-1]) + "};")
        self.output_file.close()

        self.output_file = open(self.output_file_path, 'rb')
        print(self.output_file.read())

        # Compress the injested file and append to output file
        # raw_file = open(self.input_file_path, 'rb')
        # raw_file_bytes = raw_file.read()
        # compressed = gzip.compress(raw_file_bytes)
        # # compressed = base64.b64encode(compressed_file)
        # print(compressed)
        # output_file = open(self.output_file_path, 'a')
        # for char in compressed[:-1]:
        #     output_file.write(hex(char)+",")
        # # Write last byte and closing semi-colon
        # output_file.write(hex(compressed[-1]))
        # output_file.write("};")
        # output_file.close()


cwd = os.getcwd()
html_file = FileConverter(cwd, "index.html", "CustomDRChtml.h")
css_file = FileConverter(cwd, "pico.min.css", "CustomDRCcss.h")
js_file = FileConverter(cwd, "index.js", "CustomDRCjs.h")

html_file.compress_file()
css_file.compress_file()
js_file.compress_file()
