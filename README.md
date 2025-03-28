## JOELL KEBRET TESTING CHECKLIST
*1275379*

## For the Login
In order to test my UI for complete functionality I went through these steps

checking if the login allowed invalid credentials

checking if the login had a popup when invalid credentials were entered

i.e. I tested all combinations of user pass or db name being wrong to ensure security.

## FOR Creating Cards

I first checked if the buttons worked,

then i checked if the creating a card with a null filename or contact would work

then I checked using an invalid extension on the vcf file

then i checked if the list would update upon creating a valid file

then i checked if it matched the correct .vcf or .vcard file format ie CRLF and start end

## For editing cards

I checked if it would allow users to change the name to null

No other testing is needed all contact names that arent empty or null or valid

## For checking display

I checked if it would display all the correct properties of the cards
To do this i used all the cards denoted valid from assignment two and placed them into the cards
folder for testing. I checked and counted the optional properties. the name and filename.
I also checked if the bday/ann dates and times were correct and if they were written properly if the user chose text and that utc was specified in the UI if it was shown on the card by 'Z'


## Checking exit

I tested this by seeing if the user pressing exit on the main view worked or if pressing esc on any tab would work.

## Checking DB queries

First I checked if the tables were being created on mariaDB by looking at the existence of the tables

then I checked if the database was being loaded with all the valid vcard using the same procedure I used "SELECT * FROM CONTACT" and the same for file to see if the DB was populated

I also had to check if when a valid file was deleted if it would reflect in the contact table, meaning that contact and file would have the same number of entries in their respective tables.

I also checked if files would be updated mid run meaning if i made a file during the runtime if it would be updated in the list and the db accordingly.

To test if display all and find june worked. I used a bunch of test vcard files that I made to see if they would all be displayed and all their valid card info would be given on the display when the button was pressed.

I also had to make sure only the datetimes that were complete with both date and time, not including text or utc were shown on the display all call.

to make sure the order by name was right i checked and implemented different files with different name lettering to see if the order  was right

As for testing find june, i simply tested if they were ordered by age using the same criteria as above. but only ensuring that the birthday shows and the name.


