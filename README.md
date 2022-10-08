<div id="top"></div>

<!-- PROJECT [othneildrew] SHIELDS -->

<!-- PROJECT LOGO -->
<br />
<div align="center">
  <h2 align="center">Compiler Toolchain - Jovan Vukić</h2>

  <p align="center">
    An emulator for an abstract RISC instruction set and a toolchain consisting of an assembler and a linker that can compile the specified assembly language and produce a binary file, ready to be loaded and executed in the emulator.
    <br />
    <a href="https://github.com/jovan-vukic/compiler-toolchain"><strong>Explore the project »</strong></a>
    <br />
    <br />
    <a href="https://github.com/jovan-vukic/compiler-toolchain/issues">Report Bug</a>
    ·
    <a href="https://github.com/jovan-vukic/compiler-toolchain/issues">Request Feature</a>
  </p>
</div>

<!-- TABLE OF CONTENTS -->
<details>
  <summary>Table of Contents</summary>
  <ol>
    <li>
      <a href="#about-the-project">About The Project</a>
    </li>
    <li>
      <a href="#getting-started">Getting Started</a>
      <ul>
        <li><a href="#prerequisites">Prerequisites</a></li>
        <li><a href="#installation">Installation</a></li>
        <li><a href="#expected-output">Expected Output</a></li>
      </ul>
    </li>
    <li><a href="#usage">Usage</a></li>
    <li><a href="#contributing">Contributing</a></li>
    <li><a href="#license">License</a></li>
    <li><a href="#contact">Contact</a></li>
    <li><a href="#acknowledgments">Acknowledgments</a></li>
  </ol>
</details>

<!-- ABOUT THE PROJECT -->
## About The Project

An implementation of an emulator for an abstract RISC instruction set and a toolchain consisting of an assembler and a linker that can compile the specified assembly language and produce a binary file, ready to be loaded and executed in the emulator.

<p align="right">(<a href="#top">back to top</a>)</p>

<!-- GETTING STARTED -->
## Getting Started

To get a local copy up and running follow these simple steps.

### Prerequisites

List of things you need to do:

* [download](https://bit.ly/3VoFzxM) & install an already equipped linux virtual machine with pre-installed Visual Studio Code,

* open the virtual machine and run Visual Studio Code (login password: etf).

### Installation

Setup & execution:

1. Clone the repo:
   ```sh
   git clone https://github.com/jovan-vukic/compiler-toolchain.git
   ```
2. Type the following into the terminal in VS Code:
   ```sh
   ./compile.sh
   ```
3. Move to the `tests` folder and type in the terminal:
   ```sh
   ./start.sh
   ```


### Expected Output

```sh
Emulated processor executed halt instruction
Emulated processor state: psw=0b0110000000000000
r0=0xabcd    r1=0x0001    r2=0x0002    r3=0x0003
r4=0x0004    r5=0x0005    r6=0x0000    r7=0x012a
```



<p align="right">(<a href="#top">back to top</a>)</p>

<!-- USAGE EXAMPLES -->
## Usage

**Assembler usage**
```sh
$ {ASSEMBLER} -o <output_file> <input_file>
```

|Option |Explanation                                  |
|-------|---------------------------------------------|
|-o file|Specify relocatable object output file       |

**Linker usage**
```sh
$ {LINKER} -hex -o <output_file> <input_files>
```

|Option |Explanation                                            |
|-------|-------------------------------------------------------|
|-o file|Specify output file                                    |
|-hex   |Create executable .hex  output file                    |

**Emulator usage**
```sh
$ {EMULATOR} <input_file>
```

<p align="right">(<a href="#top">back to top</a>)</p>

<!-- CONTRIBUTING -->
## Contributing

Contributions are what make the open source community such an amazing place to learn, inspire, and create. Any contributions you make are **greatly appreciated**.

If you have a suggestion that would make this better, please fork the repo and create a pull request. You can also simply open an issue with the tag "enhancement".
Don't forget to give the project a star! Thanks again!

1. Fork the Project
2. Create your Feature Branch (`git checkout -b feature/AmazingFeature`)
3. Commit your Changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the Branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

<p align="right">(<a href="#top">back to top</a>)</p>

<!-- LICENSE -->
## License

Distributed under the MIT License. See `LICENSE.md` for more information.

<p align="right">(<a href="#top">back to top</a>)</p>

<!-- CONTACT -->
## Contact

Jovan - [@jovan-vukic](https://github.com/jovan-vukic)

Project Link: [https://github.com/jovan-vukic/compiler-toolchain](https://github.com/jovan-vukic/compiler-toolchain)

<p align="right">(<a href="#top">back to top</a>)</p>

<!-- ACKNOWLEDGMENTS -->
## Acknowledgments

Used resources:

* [The full specification of the project in Serbian](./docs/doc%20(v1.2).pdf)

<p align="right">(<a href="#top">back to top</a>)</p>
