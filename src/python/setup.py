#!/usr/bin/env python

from setuptools import setup, find_packages

REQUIRES = [
    'helics~=3.2.1',
    'pyzmq',
]

ENTRIES = {
    'console_scripts' : [
        'ot-sim-io-module = otsim.io.io:main',
    ]
}

setup(
    name                 = 'otsim',
    version              = '0.0.1',
    description          = 'OT-sim Python modules',
    license              = 'GPLv3 License',
    platforms            = 'Linux',
    classifiers          = [
        'License :: OSI Approved :: GPLv3 License',
        'Development Status :: 4 - Beta',
        'Operating System :: POSIX :: Linux',
        'Programming Language :: Python :: 3.5',
        'Intended Audience :: Developers',
        'Natural Language :: English',
    ],
    entry_points         = ENTRIES,
    packages             = find_packages(),
    install_requires     = REQUIRES,
    include_package_data = True,

    package_data = {
        # Include mako template files found in all packages.
        "": ["*.mako"]
    }
)
