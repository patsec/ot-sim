#!/usr/bin/env python

import platform

from setuptools import setup, find_packages

REQUIRES = [
    "numpy",
    "pandas",
    "pyzmq",
    "requests",
    "RPI.GPIO; sys_platform=='linux' and platform_machine=='aarch64'",
    "windpowerlib",
]

SCRIPTS = [
    "ot-sim-ground-truth-module = otsim.ground_truth.ground_truth:main",
    "ot-sim-rpi-gpio-module = otsim.rpi_gpio.rpi_gpio:main",
    "ot-sim-wind-turbine-anemometer-module = otsim.wind_turbine.anemometer.anemometer:main",
    "ot-sim-wind-turbine-power-output-module = otsim.wind_turbine.power_output.power_output:main",
]

if platform.machine() == "x86_64":
    # HELICS won't build on ARM...
    REQUIRES.append("helics~=3.4.0")
    SCRIPTS.append("ot-sim-io-module = otsim.io.io:main")

ENTRIES = {
    "console_scripts": SCRIPTS,
}

setup(
    name="otsim",
    version="0.0.1",
    description="OT-sim Python modules",
    license="GPLv3 License",
    platforms="Linux",
    classifiers=[
        "License :: OSI Approved :: GPLv3 License",
        "Development Status :: 4 - Beta",
        "Operating System :: POSIX :: Linux",
        "Programming Language :: Python :: 3.5",
        "Intended Audience :: Developers",
        "Natural Language :: English",
    ],
    entry_points=ENTRIES,
    packages=find_packages(),
    install_requires=REQUIRES,
    include_package_data=True,
    package_data={
        # Include mako template files found in all packages.
        "": ["**/*.mako"]
    },
)
