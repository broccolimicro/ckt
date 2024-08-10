# hseenc

A state encoder and conflict solver for HSE

**Usage**: `hseenc [options] file...`

**General Options**:
 - `-h`,`--help`      Display this information
 - `--version`   Display version information
 - `-v`,`--verbose`   Display verbose messages
 - `-d`,`--debug`     Display internal debugging messages

**Conflict Checking**:
 - `-c`             check for state conflicts that occur regardless of sense
 - `-cu`            check for state conflicts that occur due to up-going transitions
 - `-cd`            check for state conflicts that occur due to down-going transitions
 - `-s`             check for potential state conflicts that occur regardless of sense
 - `-su`            check for potential state conflicts that occur due to up-going transitions
 - `-sd`            check for potential state conflicts that occur due to down-going transitions

## Interactive Environment

`<arg>` specifies a required argument
`(arg=value)` specifies an optional argument with a default value

**General**:
 - `help`, `h`                       print this message
 - `quit`, `q`                       exit the interactive simulation environment
 - `load (filename)`               load an hse, default is to reload current file
 - `save (filename)`               save an hse, default is to overwrite current file

**Viewing and Manipulating State**:
 - `elaborate, e`                  elaborate the predicates
 - `conflicts, c`                  check for state conflicts that occur regardless of sense
 - `conflicts up, cu`              check for state conflicts that occur due to up-going transitions
 - `conflicts down, cd`            check for state conflicts that occur due to down-going transitions
 - `suspects, s`                   check for potential state conflicts that occur regardless of sense
 - `suspects up, su`               check for potential state conflicts that occur due to up-going transitions
 - `suspects down, sd`             check for potential state conflicts that occur due to down-going transitions

**Viewing and Manipulating Structure**:
 - `print, p`                      print the current hse
 - `insert <expr>`                 insert the transition <expr> into the hse
 - `pinch <node>`                  remove <node> and merge its input and output nodes to maintain token flow

## License

Licensed by Cornell University under GNU GPL v3.

Written by Ned Bingham.
Copyright © 2020 Cornell University.

Haystack is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Haystack is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

A copy of the GNU General Public License may be found in COPYRIGHT.
Otherwise, see <https://www.gnu.org/licenses/>.

