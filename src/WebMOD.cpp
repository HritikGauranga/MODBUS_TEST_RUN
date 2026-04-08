#include <LittleFS.h>

// -------------------------------------------------------------------
// LIST FILES
// -------------------------------------------------------------------
void listFiles() {
  Serial.println("\n--- Files in LittleFS ---");

  File root = LittleFS.open("/");
  File file = root.openNextFile();

  bool found = false;

  while (file) {
    size_t size = file.size();

    Serial.print("FILE: ");
    Serial.print(file.name());
    Serial.print("   SIZE: ");
    Serial.print(size);
    Serial.print(" bytes");

    if (size > 1024 * 1024) {
      Serial.printf(" (%.2f MB)", size / 1024.0 / 1024.0);
    } else if (size > 1024) {
      Serial.printf(" (%.2f KB)", size / 1024.0);
    }

    Serial.println();

    file = root.openNextFile();
    found = true;
  }

  if (!found) Serial.println("No files found!");

  Serial.println("---------------------------\n");
}

// -------------------------------------------------------------------
// CREATE NEW FILE
// -------------------------------------------------------------------
void createFile(String name) {
  String path = "/" + name;
  File f = LittleFS.open(path, "w");

  if (!f) {
    Serial.println("Failed to create file!");
    return;
  }

  Serial.println("Enter file content (end with @):");

  String data = "";
  while (true) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == '@') break;
      data += c;
    }
  }

  f.print(data);
  f.close();
  Serial.println("File saved successfully!");
}

// -------------------------------------------------------------------
// READ FILE
// -------------------------------------------------------------------
void readFile(String name) {
  String path = "/" + name;

  if (!LittleFS.exists(path)) {
    Serial.println("File not found!");
    return;
  }

  File f = LittleFS.open(path, "r");

  Serial.println("\n--- File Content ---");
  while (f.available()) Serial.write(f.read());
  Serial.println("\n---------------------\n");

  f.close();
}

// -------------------------------------------------------------------
// DELETE FILE
// -------------------------------------------------------------------
void deleteFile(String name) {
  String path = "/" + name;

  if (!LittleFS.exists(path)) {
    Serial.println("File does not exist!");
    return;
  }

  if (LittleFS.remove(path)) {
    Serial.println("File deleted successfully!");
  } else {
    Serial.println("Failed to delete file!");
  }
}

// -------------------------------------------------------------------
// EDIT FILE
// -------------------------------------------------------------------
void editFile(String name) {
  String path = "/" + name;

  if (!LittleFS.exists(path)) {
    Serial.println("File does not exist!");
    return;
  }

  File f = LittleFS.open(path, "w");

  if (!f) {
    Serial.println("Failed to open file for editing!");
    return;
  }

  Serial.println("Enter new file content (end with @):");

  String data = "";
  while (true) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == '@') break;
      data += c;
    }
  }

  f.print(data);
  f.close();
  Serial.println("File edited successfully!");
}

// -------------------------------------------------------------------
// SETUP
// -------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Mounting LittleFS...");

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed!");
    return;
  }

  Serial.println("LittleFS Mounted Successfully!\n");

  Serial.println("Commands:");
  Serial.println(" create        → create a file");
  Serial.println(" read <name>   → read a file");
  Serial.println(" list          → list all files");
  Serial.println(" delete <name> → delete a file");
  Serial.println(" edit <name>   → edit a file");
  Serial.println("-----------------------------------\n");
}

// -------------------------------------------------------------------
// INPUT HANDLER
// -------------------------------------------------------------------
String input = "";

void loop() {
  if (Serial.available()) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      input.trim();

      if (input == "create") {
        Serial.println("\nEnter file name (example: data.txt):");
        input = "";

        while (input == "") {
          if (Serial.available()) {
            input = Serial.readStringUntil('\n');
            input.trim();
          }
        }

        createFile(input);
      }

      else if (input == "list") {
        listFiles();
      }

      else if (input.startsWith("read ")) {
        readFile(input.substring(5));
      }

      else if (input.startsWith("delete ")) {
        deleteFile(input.substring(7));
      }

      else if (input.startsWith("edit ")) {
        editFile(input.substring(5));
      }

      else if (input.length() > 0) {
        Serial.println("Unknown command!");
      }

      input = "";
    }
    else {
      input += c;
    }
  }
}
