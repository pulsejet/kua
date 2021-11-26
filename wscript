# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

from waflib import Context, Logs, Utils
import os, subprocess

VERSION = '0.0.0'
APPNAME = 'kua'
BUGREPORT = ''
URL = ''
GIT_TAG_PREFIX = 'KUA-'

def options(opt):
    opt.load(['compiler_cxx', 'gnu_dirs'])
    opt.load(['default-compiler-flags', 'boost'],
             tooldir=['.waf-tools'])

    optgrp = opt.add_option_group('Kua Options')

    optgrp.add_option('--with-tests', action='store_true', default=False,
                      help='Build unit tests')
    optgrp.add_option('--with-other-tests', action='store_true', default=False,
                      help='Build other tests')

def configure(conf):
    conf.load(['compiler_cxx', 'gnu_dirs',
               'default-compiler-flags', 'boost'])

    conf.env.WITH_TESTS = conf.options.with_tests
    conf.env.WITH_OTHER_TESTS = conf.options.with_other_tests

    conf.find_program('bash', var='BASH')
    conf.find_program('dot', var='DOT', mandatory=False)

    conf.check_cfg(package='libndn-cxx', args=['--cflags', '--libs'], uselib_store='NDN_CXX',
                   pkg_config_path=os.environ.get('PKG_CONFIG_PATH', '%s/pkgconfig' % conf.env.LIBDIR))

    conf.check_cfg(package='libndn-svs', args=['--cflags', '--libs'], uselib_store='NDN_SVS',
                   pkg_config_path=os.environ.get('PKG_CONFIG_PATH', '%s/pkgconfig' % conf.env.LIBDIR))

    boost_libs = ['system', 'thread', 'program_options', 'log_setup', 'log']
    if conf.env.WITH_TESTS or conf.env.WITH_OTHER_TESTS:
        boost_libs.append('unit_test_framework')

    conf.check_boost(lib=boost_libs, mt=True)
    if conf.env.BOOST_VERSION_NUMBER < 105800:
        conf.fatal('The minimum supported version of Boost is 1.65.1.\n'
                   'Please upgrade your distribution or manually install a newer version of Boost.\n'
                   'For more information, see https://redmine.named-data.net/projects/nfd/wiki/Boost')
    elif conf.env.BOOST_VERSION_NUMBER < 106501:
        Logs.warn('WARNING: Using a version of Boost older than 1.65.1 is not officially supported and may not work.\n'
                  'If you encounter any problems, please upgrade your distribution or manually install a newer version of Boost.\n'
                  'For more information, see https://redmine.named-data.net/projects/nfd/wiki/Boost')

    conf.check_compiler_flags()

    conf.define_cond('WITH_TESTS', conf.env.WITH_TESTS)
    conf.define_cond('WITH_OTHER_TESTS', conf.env.WITH_OTHER_TESTS)
    # The config header will contain all defines that were added using conf.define()
    # or conf.define_cond().  Everything that was added directly to conf.env.DEFINES
    # will not appear in the config header, but will instead be passed directly to the
    # compiler on the command line.
    conf.write_config_header('core/config.hpp', define_prefix='KUA_')

def build(bld):
    kua_objects = bld.objects(
        target='kua-objects',
        source=bld.path.ant_glob('src/**/*.cpp',
                                 excl=['src/kua.cpp', 'src/client.cpp']),
        use='NDN_CXX NDN_SVS BOOST',
        includes='kua',
        export_includes='kua')

    bld.program(name='kua',
                target='bin/kua',
                source='src/kua.cpp',
                use='kua-objects NDN_CXX NDN_SVS BOOST')

    bld.program(name='kua-master',
                target='bin/kua-master',
                source='src/kua.cpp',
                defines='KUA_IS_MASTER',
                use='kua-objects NDN_CXX NDN_SVS BOOST')

    bld.program(name='kua-client',
                target='bin/kua-client',
                source='src/client.cpp',
                use='kua-objects NDN_CXX NDN_SVS BOOST')
