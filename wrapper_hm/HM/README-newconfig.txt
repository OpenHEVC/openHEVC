The 0.5 TMuC release introduces new unified config file and command line
parsing code.  This allows all configuration file options to be specified
on the command line and permits the use of multiple config files.

Compatability with command line options in previous TMuC releases is
maintained with the following exceptions:
  - Commandline -1/-0 FEN does not implicitly set ASR,
    this now matches the behaviour of the config file.
  - FramesToBeEncoded is now the preferred name for FrameToBeEncoded
    (the old option still exists for compatibility)
  - Commandline & Config, setting of GOPSize nolonger unconditionally
    sets RateGOPSize.  If RateGOPSize is never set, it assumes as its
    default value, the value of GOPSize.

    Unless it is specifically required, do not set the RateGOPSize to a
    value other than -1.  This value (the default) causes RateGOPSize
    to inherit the final value of GOPSize.  While setting config files
    to have RateGOPSize = GOPSize does no immediate harm, it causes
    confusion when GOPSize is altered (without altering RateGOPSize) and
    behaviour changes relating to GPB.

All configuration options may be specified on the command line using the
following syntax:
  --CfgOptionName=value
For example,
  --InputFile=Kimono1_1920x1080_24.yuv

A list of all options avaliable is provided by running the encoder with
either no options, or the option "--help".

The command line is evaluated in the order of options given, for example:
    ./encoder -c file1.cfg --UseFoo=7 -c file2.cfg

The following may be observed:
 - file2.cfg overrides any arguments set in file1.
 - file1.cfg overrides any default arguments
 - if file2.cfg specifies "UseFoo", this value will be used
   otherwise, "UseFoo" will have the value 7.

====================
Notes for developers
====================
The new unified config file and command line parsing code allows all
configuration options, storage location, defaults, help text to be specified
in a single place.  No custom value parsing code is required.

Options are specified in TAppEncCfg::parseCfg() using the following syntax:
{{{
  /* storage for options */
  int storage_variable_int;
  unsigned storage_variable_unsigned;
  float storage_variable_float;
  bool storage_variable_bool;
  string storage_variable_string;

  /* set up configuration */
  namespace po = df::program_options_lite;
  po::Options opts;
  opts.addOptions()
  /*( option spec  , reference to storage,    default,        help text)*/
    ("option_spec0", storage_variable_int,       -42,        "help text")
    ("option_spec1", storage_variable_unsigned,   17u,       "help text")
    ("option_spec2", storage_variable_bool,     true,        "help text")
    ("option_spec3", storage_variable_float,     4.0f,       "help text")
    ("option_spec4", storage_variable_string, string("foo"), "help text")
    ;
}}}

NB, the help text is optional.

Where, the option_spec is a string containing comma separated names in
the following forms:
  - multi-charcter names are longopts that are handled in gnu style
    (and may be handled in a config file)
  - single-character names are short opts that are handled in posix style
    (and are not handled in a config file)
    prefixing a multi-character name stops it being handled in the config.

For example:
     option spec | config file formats | command line formats
       "Name"    | Name:value          | --Name=value
       "n"       | --none--            | -n value
       "-name"   | --none--            | -name value
       "Name,n"  | Name:value          | "--Name=value" or "-n value"

Caveats:
 - The default values need to be specified in the same type as the storage
   variable.  Eg, an unsigned int, would need to be specified as "17u" not
   "17"

Help text formatting:
 - Help text will be automatically wrapped and aligned if longer than the
   available space.
 - To force wrapping at a particular point, insert a newline character '\n'
   Eg: "Foo values:\n  value1 - a\n  value2 - b\n  value3 - c"
   Gives:
       Foo values:
         value1 - a
         value2 - b
         value3 - c

Please report any issues, or requests for support with the configuration to:
  David Flynn <davidf@rd.bbc.co.uk>
