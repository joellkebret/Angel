#!/usr/bin/env python3
from asciimatics.widgets import Frame, ListBox, Layout, Divider, Text, Button, Widget, PopUpDialog
from asciimatics.scene import Scene
from asciimatics.screen import Screen
from asciimatics.exceptions import ResizeScreenError, NextScene, StopApplication
import sys, os
from ctypes import *
from asciimatics.event import KeyboardEvent, MouseEvent
import mysql.connector
from mysql.connector import Error
import datetime

# Load library
vcardlib = cdll.LoadLibrary("./libvcparser.so")

# Structures

class Node(Structure):
    pass

Node._fields_ = [
    ("data", c_char_p),
    ("next", POINTER(Node))
]
class Parameter(Structure):
    _fields_ = [
        ("name", c_char_p),
        ("value", c_char_p)
    ]

class Property(Structure):
    pass 

class List(Structure):
    pass

List._fields_ = [
    ("head", c_void_p),
    ("tail", c_void_p),
    ("length", c_int)
]

Property._fields_ = [
    ("name", c_char_p),
    ("group", c_char_p),
    ("parameters", POINTER(List)),
    ("values", POINTER(List))
]

class DateTime(Structure):
    _fields_ = [
        ("UTC", c_bool),
        ("isText", c_bool),
        ("date", c_char_p),
        ("time", c_char_p),
        ("text", c_char_p)
    ]

class Card(Structure):
    _fields_ = [
        ("fn", POINTER(Property)),
        ("optionalProperties", POINTER(List)),
        ("birthday", POINTER(DateTime)),
        ("anniversary", POINTER(DateTime))
    ]

# Bindings
vcardlib.createCard.argtypes = [c_char_p, POINTER(POINTER(Card))]
vcardlib.createCard.restype = c_int
vcardlib.validateCard.argtypes = [POINTER(Card)]
vcardlib.validateCard.restype = c_int
vcardlib.writeCard.argtypes = [c_char_p, POINTER(Card)]
vcardlib.writeCard.restype = c_int
vcardlib.deleteCard.argtypes = [POINTER(Card)]
vcardlib.updateFN.argtypes = [POINTER(Card), c_char_p]
vcardlib.updateFN.restype = c_int
vcardlib.errorToString.argtypes = [c_int]
vcardlib.errorToString.restype = c_char_p

class VCardModel:
    def __init__(self):
        self.cards_dir = 'cards/'
        self.vcard_files = []
        self.current_file = None
        self.db_connection = None  
        self.load_vcards()

    def load_vcards(self):
        self.vcard_files.clear()

        #get current files 
        current_files = set()
        for file in os.listdir(self.cards_dir):
            if file.endswith(('.vcf', '.vcard')):
                current_files.add(file)

        #check for entries that are delete so delete contact
        if self.db_connection and self.db_cursor:
            #get file names in the table
            self.db_cursor.execute("SELECT file_name FROM FILE")
            db_files = {row[0] for row in self.db_cursor.fetchall()}

            # check nonexistitng files
            stale_files = db_files - current_files
            if stale_files:
                self.db_cursor.executemany(
                    "DELETE FROM FILE WHERE file_name = %s",
                    [(file,) for file in stale_files]
                )
                self.db_connection.commit()

        #load all cards after deleting
        if not self.db_connection or not self.db_cursor:
            for file in os.listdir(self.cards_dir):
                if file.endswith(('.vcf', '.vcard')):
                    card_ptr = POINTER(Card)()
                    res = vcardlib.createCard(f"{self.cards_dir}{file}".encode(), byref(card_ptr))
                    if res == 0 and vcardlib.validateCard(card_ptr) == 0:
                        self.vcard_files.append((file, file))
                    if res == 0:  # Only delete if createCard succeeded
                        vcardlib.deleteCard(card_ptr)
            return

        #free memory 
        for file in os.listdir(self.cards_dir):
            if file.endswith(('.vcf', '.vcard')):
                filepath = f"{self.cards_dir}{file}"
                card_ptr = POINTER(Card)()
                res = vcardlib.createCard(filepath.encode(), byref(card_ptr))
                if res == 0 and vcardlib.validateCard(card_ptr) == 0:
                    self.vcard_files.append((file, file))
                    self._populate_db(filepath, card_ptr)
                if res == 0: 
                    vcardlib.deleteCard(card_ptr)

    def get_summary(self):
        return self.vcard_files

    #null file
    def get_current_contact(self):
        if not self.current_file:
            return {"filename": "", "contact": "", "birthday": "", "anniversary": "", "other_props": "0"}

        card_ptr = POINTER(Card)()
        res = vcardlib.createCard(f"{self.cards_dir}{self.current_file}".encode(), byref(card_ptr))
        if res != 0:
            return {}

        card = card_ptr.contents

        #contact must exist before dereferencing
        contact_str = ""
        if card.fn and card.fn.contents and card.fn.contents.values and card.fn.contents.values.contents:
            node_ptr = cast(card.fn.contents.values.contents.head, POINTER(Node))
            if node_ptr and node_ptr.contents and node_ptr.contents.data:
                contact_str = node_ptr.contents.data.decode()

        optional_count = 0
        if card.optionalProperties:
            optional_list = card.optionalProperties.contents
            optional_count = optional_list.length if optional_list.length >= 0 else 0

        details = {
            "filename": self.current_file,
            "contact": contact_str,
            "birthday": (
                (f"Date: {card.birthday.contents.date.decode()} " if card.birthday and card.birthday.contents.date and card.birthday.contents.date.decode() else "") +
                (f"Time: {card.birthday.contents.time.decode()}{' (UTC)' if card.birthday.contents.UTC else ''}" if card.birthday and card.birthday.contents.time and card.birthday.contents.time.decode() else "") +
                (f"Text: {card.birthday.contents.text.decode()}" if card.birthday and card.birthday.contents.isText and card.birthday.contents.text and card.birthday.contents.text.decode() else "")
            ).strip(),
            "anniversary": (
                (f"Date: {card.anniversary.contents.date.decode()} " if card.anniversary and card.anniversary.contents.date and card.anniversary.contents.date.decode() else "") +
                (f"Time: {card.anniversary.contents.time.decode()}{' (UTC)' if card.anniversary.contents.UTC else ''}" if card.anniversary and card.anniversary.contents.time and card.anniversary.contents.time.decode() else "") +
                (f"Text: {card.anniversary.contents.text.decode()}" if card.anniversary and card.anniversary.contents.isText and card.anniversary.contents.text and card.anniversary.contents.text.decode() else "")
            ).strip(),
            "other_props": str(optional_count)
        }


        vcardlib.deleteCard(card_ptr)
        return details

    def update_current_contact(self, details):
        contact_name = details["contact"].strip()
        if not contact_name:
            return

        filepath = None
        if not self.current_file or self.current_file == "":
            #new card
            filename = details["filename"]
            filepath = f"{self.cards_dir}{filename}"
            if os.path.exists(filepath):
                return
            with open(filepath, "w") as f:
                f.write("BEGIN:VCARD\r\n")
                f.write("VERSION:4.0\r\n")
                f.write(f"FN:{contact_name}\r\n")
                f.write("END:VCARD\r\n")
            card_ptr = POINTER(Card)()
            res = vcardlib.createCard(filepath.encode(), byref(card_ptr))
            if res != 0:
                return
            self.current_file = filename
        else:
            #card exists
            filepath = f"{self.cards_dir}{self.current_file}"
            card_ptr = POINTER(Card)()
            res = vcardlib.createCard(filepath.encode(), byref(card_ptr))
            if res != 0:
                return
            res = vcardlib.updateFN(card_ptr, contact_name.encode())
            if res != 0:
                vcardlib.deleteCard(card_ptr)
                return f"Failed to update contact name: {vcardlib.errorToString(res).decode()}"

        # validate before writing to disk
        valid = vcardlib.validateCard(card_ptr)
        if valid != 0:
            vcardlib.deleteCard(card_ptr)
            return

        write_res = vcardlib.writeCard(filepath.encode(), card_ptr)
        if write_res == 0 and self.db_connection and self.db_cursor:
            self._populate_db(filepath, card_ptr)  # update database
        
       #reload the list after
        vcardlib.deleteCard(card_ptr)
        self.load_vcards()

        return

    def connect_to_db(self, username, password, db_name):
        try:
            self.db_connection = mysql.connector.connect(
                host="dursley.socs.uoguelph.ca",
                user=username,
                password=password,
                database=db_name
            )
            self.db_cursor = self.db_connection.cursor()
            self.create_tables()  
            self.load_vcards()  #populate and load table
            return True
        except Error as e:
            self.db_connection = None
            self.db_cursor = None
            return str(e)
    
    def create_tables(self):
        try:
            # FILE table
            self.db_cursor.execute("""
                CREATE TABLE IF NOT EXISTS FILE (
                    file_id INT AUTO_INCREMENT PRIMARY KEY,
                    file_name VARCHAR(60) NOT NULL,
                    last_modified DATETIME,
                    creation_time DATETIME NOT NULL
                )
            """)

            # CONTACT table
            self.db_cursor.execute("""
                CREATE TABLE IF NOT EXISTS CONTACT (
                    contact_id INT AUTO_INCREMENT PRIMARY KEY,
                    name VARCHAR(256) NOT NULL,
                    birthday DATETIME,
                    anniversary DATETIME,
                    file_id INT NOT NULL,
                    FOREIGN KEY (file_id) REFERENCES FILE(file_id) ON DELETE CASCADE
                )
            """)
            self.db_connection.commit()
        except Error as e:
            raise Exception(f"Error creating tables: {e}")
    
    def _populate_db(self, filepath, card_ptr):
        card = card_ptr.contents
        filename = os.path.basename(filepath)
        
        # check if file exists
        self.db_cursor.execute("SELECT file_id, last_modified FROM FILE WHERE file_name = %s", (filename,))
        file_result = self.db_cursor.fetchone()

        # get time
        last_modified = datetime.datetime.fromtimestamp(os.path.getmtime(filepath))
        last_modified = last_modified.replace(microsecond=0)
        creation_time = datetime.datetime.now() if not file_result else file_result[1]

        file_id = None
        if not file_result:
            #inster files if not in table already
            self.db_cursor.execute("""
                INSERT INTO FILE (file_name, last_modified, creation_time)
                VALUES (%s, %s, %s)
            """, (filename, last_modified, creation_time))
            self.db_connection.commit()
            file_id = self.db_cursor.lastrowid
        else:
            file_id = file_result[0]
            #update last modified time
            db_last_modified = file_result[1]
            if db_last_modified != last_modified:
                self.db_cursor.execute("""
                    UPDATE FILE SET last_modified = %s WHERE file_id = %s
                """, (last_modified, file_id))
                self.db_connection.commit()

        #get details
        contact_name = ""
        if card.fn and card.fn.contents.values and card.fn.contents.values.contents.head:
            node_ptr = cast(card.fn.contents.values.contents.head, POINTER(Node))
            if node_ptr and node_ptr.contents.data:
                contact_name = node_ptr.contents.data.decode()

        birthday = None
        if (card.birthday and 
            not card.birthday.contents.isText and 
            card.birthday.contents.date and 
            card.birthday.contents.time):  # Require both date and time
            date_str = card.birthday.contents.date.decode()
            time_str = card.birthday.contents.time.decode()
            try:
                birthday = datetime.datetime.strptime(f"{date_str}{time_str}", "%Y%m%d%H%M%S")
            except ValueError:
                birthday = None

        anniversary = None
        if (card.anniversary and 
            not card.anniversary.contents.isText and 
            card.anniversary.contents.date and 
            card.anniversary.contents.time):  # Require both date and time
            date_str = card.anniversary.contents.date.decode()
            time_str = card.anniversary.contents.time.decode()
            try:
                anniversary = datetime.datetime.strptime(f"{date_str}{time_str}", "%Y%m%d%H%M%S")
            except ValueError:
                anniversary = None

        self.db_cursor.execute("""
            SELECT contact_id FROM CONTACT WHERE file_id = %s
        """, (file_id,))
        contact_result = self.db_cursor.fetchone()

        if contact_result:
            # update existing contact
            self.db_cursor.execute("""
                UPDATE CONTACT 
                SET name = %s, birthday = %s, anniversary = %s
                WHERE file_id = %s
            """, (contact_name, birthday, anniversary, file_id))
        else:
            # insert new contact
            self.db_cursor.execute("""
                INSERT INTO CONTACT (name, birthday, anniversary, file_id)
                VALUES (%s, %s, %s, %s)
            """, (contact_name, birthday, anniversary, file_id))
        self.db_connection.commit()


#ui stuff
class VCardListView(Frame):
    def __init__(self, screen, model):
        super().__init__(screen, screen.height, screen.width, title="vCard List")
        self._model = model
        layout = Layout([100], fill_frame=True)
        self.add_layout(layout)
        self._list = ListBox(Widget.FILL_FRAME, model.get_summary(), name="cards", add_scroll_bar=True, on_select=self._on_select)
        layout.add_widget(self._list)
        layout.add_widget(Divider())
        buttons = Layout([1, 1, 1, 1])
        self.add_layout(buttons)
        buttons.add_widget(Button("Create", self._create), 0)
        buttons.add_widget(Button("Edit", self._edit), 1)
        buttons.add_widget(Button("DB Queries", self._db_queries), 2)
        buttons.add_widget(Button("Exit", self._quit), 3)
        self.fix()

    def _create(self):
        self._model.current_file = None
        raise NextScene("Details")

    def _edit(self):
        if self._list.value is None:
            return
        self._model.current_file = self._list.value
        raise NextScene("Details")

    def _db_queries(self):
        raise NextScene("DB")

    def _on_select(self):
        self._model.current_file = self._list.value
        raise NextScene("Details")

    def process_event(self, event):
        if isinstance(event, KeyboardEvent):
            if event.key_code == Screen.KEY_ESCAPE:
                raise StopApplication("ESC pressed")
        elif isinstance(event, MouseEvent):
            event = super().process_event(event)
            if self._list.value is not None:
                self._model.current_file = self._list.value
                raise NextScene("Details")
            return event
        return super().process_event(event)

    @staticmethod
    def _quit():
        raise StopApplication("User exited")

class VCardDetailsView(Frame):
    def __init__(self, screen, model):
        super().__init__(screen, screen.height, screen.width, title="vCard Details")
        self._model = model

        #create the layout instance
        layout = Layout([100], fill_frame=True)
        self.add_layout(layout)

        #editable fields
        self._filename = Text("File name:", "filename")
        self._contact = Text("Contact", "contact")

        #read fields
        self._birthday = Text("Birthday:", "birthday", readonly=True)
        self._anniversary = Text("Anniversary:", "anniversary", readonly=True)
        self._optional_props = Text("Optional Properties:", "other_props", readonly=True)

        # add widgets to layout
        layout.add_widget(self._filename)
        layout.add_widget(self._contact)
        layout.add_widget(self._birthday)
        layout.add_widget(self._anniversary)
        layout.add_widget(self._optional_props)

        #buttons Layout
        buttons = Layout([1, 1])
        self.add_layout(buttons)
        buttons.add_widget(Button("OK", self._save), 0)
        buttons.add_widget(Button("Cancel", self._cancel), 1)

        self.fix()

    def reset(self):
        super().reset()
        self.data = self._model.get_current_contact()

        #make read fields display correct values
        self._birthday.value = self.data.get("birthday", "")
        self._anniversary.value = self.data.get("anniversary", "")
        self._optional_props.value = self.data.get("other_props", "")

    def _save(self):
        self.save()
        res = self._model.update_current_contact(self.data)
        if res != "OK":
            pass
        raise NextScene("Main")

    def process_event(self, event):
        if isinstance(event, KeyboardEvent):
            if event.key_code == Screen.KEY_ESCAPE:
                raise StopApplication("ESC pressed")
        return super().process_event(event)

    @staticmethod
    def _cancel():
        raise NextScene("Main")

class LoginView(Frame):
    def __init__(self, screen, model):
        super().__init__(screen, screen.height, screen.width, title="User Login")
        self._model = model
        layout = Layout([100], fill_frame=True)
        self.add_layout(layout)

        self._username = Text("Username:", "username")
        self._password = Text("Password:", "password")
        self._db_name = Text("DB name:", "db_name")

        layout.add_widget(self._username)
        layout.add_widget(self._password)
        layout.add_widget(self._db_name)
        buttons = Layout([1, 1])

        self.add_layout(buttons)
        buttons.add_widget(Button("OK", self._save), 0)
        buttons.add_widget(Button("Cancel", self._cancel), 1)
        self.fix()

    def _save(self):
        self.save()
        result = self._model.connect_to_db(
            self.data["username"],
            self.data["password"],
            self.data["db_name"]
        )
        if result is True:
            raise NextScene("Main")
        else:
            self.scene.add_effect(PopUpDialog(self.screen, "Failed to connect to database.", ["OK"]))
            
    def _cancel(self):
        raise StopApplication("User cancelled login")

    def process_event(self, event):
        if isinstance(event, KeyboardEvent) and event.key_code == Screen.KEY_ESCAPE:
            raise StopApplication("ESC pressed")
        return super().process_event(event)

class VcardDB(Frame):
    def __init__(self, screen, model):
        super().__init__(screen, screen.height, screen.width, title="vCard Database Queries")
        self._model = model

        layout = Layout([100], fill_frame=True)
        self.add_layout(layout)

        self._results_list = ListBox(Widget.FILL_FRAME, [], name="results", add_scroll_bar=True)
        layout.add_widget(self._results_list)
        layout.add_widget(Divider())

        buttons = Layout([1, 1, 1])
        self.add_layout(buttons)
        
        buttons.add_widget(Button("Display All", self._display_all), 0)
        buttons.add_widget(Button("Born in June", self._find_june), 1)
        buttons.add_widget(Button("Cancel", self._cancel), 2)

        self.fix()

    def _display_all(self):
        if not self._model.db_connection or not self._model.db_cursor:
            self._show_error("No database connection.")
            return

        self._model.load_vcards()

        self._model.db_cursor.execute("""
            SELECT c.contact_id, c.name, c.birthday, c.anniversary, f.file_name
            FROM CONTACT c
            JOIN FILE f ON c.file_id = f.file_id
            ORDER BY c.name
        """)
        results = []
        
        for row in self._model.db_cursor.fetchall():
            contact_id, name, birthday, anniversary, file_name = row
            
            #list accordingly birthdat and ANN
            birthday_str = birthday.strftime("%Y-%m-%d %H:%M:%S") if birthday else "N/A"
            anniversary_str = anniversary.strftime("%Y-%m-%d %H:%M:%S") if anniversary else "N/A"

            #display string as so
            display_str = f"ID: {contact_id}, N: {name}, BDAY: {birthday_str}, ANN: {anniversary_str}, File: {file_name}"
            results.append((display_str, file_name))
        self._results_list.options = results or [("No contacts found.", None)]
        self.fix()

    def _find_june(self):
        if not self._model.db_connection or not self._model.db_cursor:
            self._show_error("No database connection.")
            return

        self._model.db_cursor.execute("""
            SELECT c.name, c.birthday, f.last_modified
            FROM CONTACT c
            JOIN FILE f ON c.file_id = f.file_id
            WHERE MONTH(c.birthday) = 6
            ORDER BY DATEDIFF(f.last_modified, c.birthday) DESC
        """)
        results = []
        rows = self._model.db_cursor.fetchall() 
        for row in rows: #this is a mess make it work ////
            name, birthday, last_modified = row
            birthday_str = birthday.strftime("%Y-%m-%d %H:%M:%S") if birthday else "N/A"
            display_str = f"{name}, Birthday: {birthday_str}"
            age_days = (last_modified - birthday).days if birthday and last_modified else 0
            results.append((display_str, None))
        self._results_list.options = results or [("No contacts born in June.", None)]

        self._model.db_cursor.execute("SELECT COUNT(*) FROM FILE")
        file_count = self._model.db_cursor.fetchone()[0]
        self._model.db_cursor.execute("SELECT COUNT(*) FROM CONTACT")
        contact_count = self._model.db_cursor.fetchone()[0]
        self.fix()

    def _show_error(self, message):
        self.scene.add_effect(PopUpDialog(self.screen, message, ["OK"]))
        
    def process_event(self, event):
        if isinstance(event, KeyboardEvent):
            if event.key_code == Screen.KEY_ESCAPE:
                raise StopApplication("ESC pressed")
        return super().process_event(event)

    @staticmethod
    def _cancel():
        raise NextScene("Main")


#the screen adding pages accordingly
def demo(screen, scene):
    model = VCardModel()
    scenes = [
        Scene([LoginView(screen, model)], -1, name="Login"),
        Scene([VCardListView(screen, model)], -1, name="Main"),
        Scene([VCardDetailsView(screen, model)], -1, name="Details"),
        Scene([VcardDB(screen, model)], -1, name="DB")
    ]
    desired_scene_name = "Login" if scene is None else scene.name
    start_scene = next((s for s in scenes if s.name == desired_scene_name), scenes[0]) 
    screen.play(scenes, stop_on_resize=True, start_scene=start_scene)

last_scene = None
while True:
    try:
        Screen.wrapper(demo, arguments=[last_scene])
        sys.exit(0)
    except ResizeScreenError as e:
        last_scene = e.scene
