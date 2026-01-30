nghttp2 Documentation
=====================

The documentation of nghttp2 is generated using Sphinx.  This
directory contains the source files to be processed by Sphinx.  The
source file for API reference is generated using a script called
``mkapiref.py`` from the nghttp2 C source code.

Generating API reference
------------------------

As described earlier, we use ``mkapiref.py`` to generate rst formatted
text of API reference from C source code.  The ``mkapiref.py`` is not
so flexible and it requires that C source code is formatted in rather
strict rules.

To generate API reference, just run ``make html``. It runs
``mkapiref.py`` and then run Sphinx to build the entire document.

The ``mkapiref.py`` reads C source code and searches the comment block
starts with ``/**``. In other words, it only processes the comment
block starting ``/**``. The comment block must end with ``*/``. The
``mkapiref.py`` requires that which type of the object this comment
block refers to.  To specify the type of the object, the next line
must contain the so-called action keyword.  Currently, the following
action keywords are supported: ``@function``, ``@functypedef``,
``@enum``, ``@struct`` and ``@union``. The following sections
describes each action keyword.

@function
#########

``@function`` is used to refer to the function.  The comment block is
used for the document for the function.  After the script sees the end
of the comment block, it consumes the lines as the function
declaration until the line which ends with ``;`` is encountered.

In Sphinx doc, usually the function argument is formatted like
``*this*``.  But in C, ``*`` is used for dereferencing a pointer and
we must escape ``*`` with a back slash. To avoid this, we format the
argument like ``|this|``. The ``mkapiref.py`` translates it with
``*this*``, as escaping ``*`` inside ``|`` and ``|`` as necessary.
Note that this shadows the substitution feature of Sphinx.

The example follows::

    /**
     * @function
     *
     * Submits PING frame to the |session|.
     */
    int nghttp2_submit_ping(nghttp2_session *session);


@functypedef
############

``@functypedef`` is used to refer to the typedef of the function
pointer. The formatting rule is pretty much the same with
``@function``, but this outputs ``type`` domain, rather than
``function`` domain.

The example follows::

    /**
     * @functypedef
     *
     * Callback function invoked when |session| wants to send data to
     * remote peer.
     */
    typedef nghttp2_ssize (*nghttp2_send_callback2)
    (nghttp2_session *session,
     const uint8_t *data, size_t length, int flags, void *user_data);

@enum
#####

``@enum`` is used to refer to the enum.  Currently, only enum typedefs
are supported.  The comment block is used for the document for the
enum type itself. To document each values, put comment block starting
with the line ``/**`` and ending with the ``*/`` just before the enum
value.  When the line starts with ``}`` is encountered, the
``mkapiref.py`` extracts strings next to ``}`` as the name of enum.

At the time of this writing, Sphinx does not support enum type. So we
use ``type`` domain for enum it self and ``macro`` domain for each
value. To refer to the enum value, use ``:enum:`` pseudo role. The
``mkapiref.py`` replaces it with ``:macro:``. By doing this, when
Sphinx will support enum officially, we can replace ``:enum:`` with
the official role easily.

The example follows::

    /**
     * @enum
     * Error codes used in the nghttp2 library.
     */
    typedef enum {
      /**
       * Invalid argument passed.
       */
      NGHTTP2_ERR_INVALID_ARGUMENT = -501,
      /**
       * Zlib error.
       */
      NGHTTP2_ERR_ZLIB = -502,
    } nghttp2_error;

@struct
#######

``@struct`` is used to refer to the struct. Currently, only struct
typedefs are supported. The comment block is used for the document for
the struct type itself.To document each member, put comment block
starting with the line ``/**`` and ending with the ``*/`` just before
the member.  When the line starts with ``}`` is encountered, the
``mkapiref.py`` extracts strings next to ``}`` as the name of struct.
The block-less typedef is also supported. In this case, typedef
declaration must be all in one line and the ``mkapiref.py`` uses last
word as the name of struct.

Some examples follow::
    
    /**
     * @struct
     * The control frame header.
     */
    typedef struct {
      /**
       * SPDY protocol version.
       */
      uint16_t version;
      /**
       * The type of this control frame.
       */
      uint16_t type;
      /**
       * The control frame flags.
       */
      uint8_t flags;
      /**
       * The length field of this control frame.
       */
      int32_t length;
    } nghttp2_ctrl_hd;
        
    /**
     * @struct
     *
     * The primary structure to hold the resources needed for a SPDY
     * session. The details of this structure is hidden from the public
     * API.
     */
    typedef struct nghttp2_session nghttp2_session;

@union
######

``@union`` is used to refer to the union. Currently, ``@union`` is an
alias of ``@struct``.
