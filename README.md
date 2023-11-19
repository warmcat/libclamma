# libclamma

This is a heavily rewritten fork of https://github.com/karpathy/llama2.c .
The original is very cool making llama2 inference available in C originally.

## Goals of this fork

This fork cleans, refactors and upgrades the C part.  It's basically
reimplementing the project around what I learned from writing and maintaining
libwebsockets over the years.

 - It's a CMake project now so you can easily build into a single shared or
   static lib, and just link your app to it.
   
 - It's usable on Linux, Mac, cross-builds and also bare metal.

 - There is a simple, stable, opaque API you can include from clamma.h.
     - You instantiate a transformer that is bound to the model checkpoint and
       token dictionary.  This is like the "static part".
     - One or more "sessions" can be created bound to the transformer, which
       can independently run a query each at a time.  These operate
       concurrently using the parent transformer in const mode, with their own
       private state (so two sessions can be generating two completely different
       stories on the same model concurrently.  These are like the
       "dynamic parts".

 - The whole library API is six functions (see inc/clamma.h)
    - creation + configuration, and destruction of the transformer
    - creation + configuration, and destruction of one or more sessions on the transformer
    - starting a query + prompt on a session
    - emitting the next token for the next session in a round-robin list
   
 - Acceleration by SMP can optionally be concealed in the query operations, if
   enabled for build this currently needs pthreads.  See CMake Build options
   section below.  pthreads support (working on Linux and Mac) is provided, but
   other thread libraries are designed to be added easily.
   
 - output tokens are converted to strings and a user-provided, per-session
   callback called for each one, along with a user-provided opque `void *` for
   the session context

 - stdint.h sized types are used for all checkpoint file parsing so it's portable,
   const pointers are used appropriately internally and externally
 
 - the code is clean for -pendantic and -Wextra -Wall -Werror
 
 - the code is valgrind-clean
 
 - all allocations are checked and fail upwards cleanly
 
 - model checkpoint / tokenizer files are are looked for first in the cwd, and
   if not found then in typically `/var/lib/clamma-models`
   
 - both `float` and `int8_q80` quantized model checkpoint files are supported
   with the decision made at runtime after examining the checkpoint file.  The
   code is cleanly unified rather than two separate implementations as in
   the original.

 - `mmap()` is not required, the transformer can be instantiated to use mmap on
   to the model checkpoint file (the default), or to use malloc allocated cached
   blocks up to a size limit, or to directly access the model from the memory
   map.
   
 - simple standalone example apps provided using the library (these are also
   built when the library is built, for convenience, but the idea is to use
   these as the starting point for your own standalone project):
    - `clamma-gen`: generate output from an optional prompt
    - `clamma-gen-multi`: generate many different outputs concurrently from an
      optional prompt, directed to /tmp/out0.txt, /tmp/out1.txt etc.  You can
      watch these growing in realtime from another terminal using
      `tail -f /tmp/out0.txt` etc
    - `clamma-chat`: same as clamma-gen, but retain the session and continue to
      prompt via stdin.  Requires the chat-specific model.
    - `clamma-ws-demo`: runs a webserver on localhost:7681 which serves two
      simultaneously created stories110M outputs when http://127.0.0.1:7681 is
      opened in a browser.  Requires current libwebsockets (from main)
 

## Getting started

### 1) Get, build and install libclamma & test apps

 - if you want the webserver demo:
     - clone libwebsockets: git clone https://libwebsockets.org/repo/libwebsockets
     - build and install
     - build libclamma with `cmake .. -DLIBCLAMMA_WITH_LWS=1 -DLIBCLAMMA_THREADING=PTHREADS`
     
 - if you want to enable pthreads on libclamma:
     - build libclamma with `cmake .. -DLIBCLAMMA_THREADING=PTHREADS`

 ```bash
  $ git clone ssh://git@github.com/warmcat/libclamma
  $ cd libclamma
  $ mkdir build && cd build
  $ cmake ..
  $ make && sudo make install
 ```
 
 This builds and installs (normally in /usr/local) libclamma.a, and also builds
 and installs the selftests and "standalone apps".

### 2) Get and process the models

There are two basic kinds of model from llama2.c: ready to use tinystories ones;
or, llama2 ones that arrive in "need of processing" (a machine with 64GB
successfully processed llama2_7b and ...7b_chat) ones.

To get started can skip dealing with the processing complications by downloading
tinystories models below.

#### 2.1) tinystories (ready to download and use)

The author of llama2.c has produced some ready-to-use model checkpoints you can
get from huggingface, which was trained on simple children's stories.  These
use the 32000 token `tokenizer.bin` dictionary in the top level of the project.

 - https://huggingface.co/karpathy/tinyllamas/resolve/main/stories110M.bin?download=true
 - https://huggingface.co/karpathy/tinyllamas/resolve/main/stories42M.bin?download=true
 - https://huggingface.co/karpathy/tinyllamas/resolve/main/stories15M.bin?download=true

There's also a 1MB model checkpoint with only 260K parameters, it's too simple
and only produces plausible gibberish, but it can and does work.

It requires a special minimized 512 token dictionary to go with it.

https://huggingface.co/karpathy/tinyllamas/resolve/main/stories260K/stories260K.bin?download=true
https://huggingface.co/karpathy/tinyllamas/resolve/main/stories260K/tok512.bin?download=true

You can proceed to step 3 below to try them just after downloading the pieces.

#### 2.2) llama2 (comes in need of processing)

1) Install the python deps to run the processing needed later

From the toplevel libclamma dir: `pip install -r requirements.txt`

2) Visit meta's website https://ai.meta.com/resources/models-and-libraries/llama-downloads/
and request a download link (which has a 24h lifetime).  The site will send you
an email with a customized URL within 20s or so.

On your machine you will use for processing, get the download script

```
$ wget https://raw.githubusercontent.com/facebookresearch/llama/main/download.sh
$ chmod +x download.sh
$ ./download.sh
```

Paste the URL from the email, and select the models you want, eg, `7B,7B-chat`.

 - The non-chat model checkpoint is used by `clamma-gen` app, it can accept an
optional prompt and produce output

 - the chat model checkpoint is used by `clamma-chat` and can accept a
 "system prompt", and a series of user prompts from stdin retaining context.

The download script will create and populate some subdirectories like
`llama-2-7b-chat/`.  Before these can be used, they have to be converted into
the .bin format we know how to parse.  They can be converted to float (26GB for
llama2 7B or 7B_chat) or int8 quantization (6GB).  `export.py` is in the
toplevel of libclamma.

Float:
```bash
$ python export.py llama2_7b.bin --meta-llama path/to/llama/model/7B
```

int8 quantized:
```bash
$ python export.py llama2_7b_q80.bin --version 2 --meta-llama path/to/llama/model/7B
```

Now the .bin files created at this step can be used with clamma-gen (llama2_7b.bin) and clamma-chat (llama2_7b_q80.bin).
 
### 3) Try the models

The transformer needs two files when it starts up - a tokenizer dictionary
and the model checkpoint (.bin file filled with model weights for the tokens in
the dictionary)

By default, the transformer looks for `tokenizer.bin` (from the toplevel of
libclamma repo) in the cwd, and in `/var/lib/clamma-models`.  You can also use
`-z <path-to-tokenizer.bin>` to direct the app where the file is.

(Note the 260K stories model is too small to produce anything other than
goldfish-memory nonsense, but it's so small it's an interesting datapoint that
it can work at all)

$ clamma-gen stories260K.bin -z tok512.bin -s 1024

```
☙ Clamma ❧  model: stories260K.bin (1MB) float MMAP, dim: 64, hidden_dim: 172, vocab_size: 512 (6KB), layers: 5, heads: 8, seq_len: 512
  Session: 0.668MB, temp: 1.000000, topp: 0.900000, seed: 1024
Lily and Kitty went to the park with the swings. They had a marble, animals, white pictures. They had a lot of fun, just like it, and even more.
One day, Kitty's money came to the living room and the swing. She said, "Wow, I can get some job jump to the swing. It is a message!"
Kitty was surprised, but she swam up and jumped. It was not a mean meat when her hand came out. She knew she could ask the sweet horse to give the kids a swing, something else. And they all lived happily ever

achieved tok/s: 10541.666667
```

$ clamma-gen stories110M.bin

```
☙ Clamma ❧  model: stories110M.bin (418MB) float MMAP, dim: 768, hidden_dim: 2048, vocab_size: 32000 (423KB), layers: 12, heads: 12, seq_len: 1024
  Session: 72.183MB, temp: 1.000000, topp: 0.900000, seed: 1701604266
Timmy loved his jeep. He enjoyed pushing it around and pretending it was a real car. But one day he wanted to drive a real car like his older brother. His brother said, "You're too little to drive, Timmy." 
But Timmy said, "I can drive! I know how!" 
So Timmy climbed into the driver's seat and sat in the driver's seat. He grabbed the steering wheel and pushed the pedals. But then the jeep rolled over and tipped over. Timmy was frightened. 
His brother ran to help him, but it was too late. Timmy was hurt badly. His brother called for help and soon enough, an ambulance came. Timmy was taken to the hospital and was taken to the hospital. 
Timmy learned a very important lesson that day: be careful when you play with your jeep. Timmy never wanted to be in a real jeep again.

achieved tok/s: 25.244595
```

$ clamma-gen llama2_7b_q80.bin -i "Cats" -s 1234

```
☙ Clamma ❧  model: llama2_7b_q80.bin (6828MB) int8 MMAP, dim: 4096, hidden_dim: 11008, vocab_size: 32000 (423KB), layers: 32, heads: 32, seq_len: 2048
  Session: 2048.627MB, temp: 1.000000, topp: 0.900000, seed: 1234
What's in a name?
When we talk about cats, as one, we mean the species Felis silvestris catus. That's an extremely ancient species, more than a million years old, and it's what we are descended from. Most of us would be better off, if our ancestors hadn't decided, thousands of years ago, to stop hunting cats for food, but nobody said these things are simple. If we follow one line of logic, we have a right to a claim on an ancient tribe.
That's nonsense. A quick look at cats through history shows how unreliable it is. During our investigations we had to look at the cats of different periods and how these cats were named. There's an obvious bias in the choice of cats used in movies, comics and literature. We should always be wary of how these fictional cats are treated.
How do you pick up an ancient, archeological cat? The answer: you don't. These skeletons are extremely rare, with each major cat exhibition or museum holding perhaps one of these. There's one in Paris, two

achieved tok/s: 0.849046
```

$ clamma-chat llama2_7b_chat_q80.bin -z tokenizer.bin -s 1234 -i "tell me about your cat" -y "Tell interesting stories in a scottish accent"

```
☙ Clamma ❧  model: llama2_7b_chat_q80.bin (6828MB) int8 MMAP, dim: 4096, hidden_dim: 11008, vocab_size: 32000 (423KB), layers: 32, heads: 32, seq_len: 2048
  Session: 2048.627MB, temp: 1.000000, topp: 0.900000, seed: 1234
Oh, lassie, ye'll no' believe the tales I could tell ye aboot my wee cat, McFluffers! *adjusts tartan scarf*

First o' a', McFluffers is a bonnie beast, wi' a coat as fluffy as a heather bloom and eyes as bright as a midges' arse. *chuckles* He's a wee scamp, that one, always gettin' intae trouble and leavin' his mark on the settee.

But the real magic happens when the night draws in and the fire pitstarts to glow. That's when McFluffers transforms intae a fierce and fearsome predator, stalkin' the halls like a highland wolf. *giggles* He's a wee bit of a scary-cat, but I luv him tae death!

I remember this one time, I was sitt

achieved tok/s: 0.676844
```

$ clamma-gen-multi stories110M.bin -i "Sally was having a nice day. " -c 8

```
☙ Clamma ❧  8 x pthreads, model: stories110M.bin (418MB) float MMAP, vocab: 32000 (423KB),
             d: 768, hd: 2048, l: 12, h: 12, kvh: 12, seq_len: 1024
  Session: 72.183MB
    Query: temp: 1.000000, topp: 0.900000, seed: 1701852025941342170
  Session: 72.183MB
    Query: temp: 1.000000, topp: 0.900000, seed: 1701852025941465145
  Session: 72.183MB
    Query: temp: 1.000000, topp: 0.900000, seed: 1701852025941525611
  Session: 72.183MB
    Query: temp: 1.000000, topp: 0.900000, seed: 1701852025941585101
  Session: 72.183MB
    Query: temp: 1.000000, topp: 0.900000, seed: 1701852025941637409
  Session: 72.183MB
    Query: temp: 1.000000, topp: 0.900000, seed: 1701852025941704594
  Session: 72.183MB
    Query: temp: 1.000000, topp: 0.900000, seed: 1701852025941755788
  Session: 72.183MB
    Query: temp: 1.000000, topp: 0.900000, seed: 1701852025941817669
  Session: 173 tokens, tok/s: 8.488
  Session: 172 tokens, tok/s: 8.438
  Session: 164 tokens, tok/s: 8.046
  Session: 196 tokens, tok/s: 9.615
  Session: 233 tokens, tok/s: 11.429
  Session: 194 tokens, tok/s: 9.516
  Session: 180 tokens, tok/s: 8.829
  Session: 175 tokens, tok/s: 8.583
```

This produced 8 stories concurrently in around 15s real time, eg

/tmp/out0.txt

```
Sally was having a nice day. Sally was looking for something special. She saw a little door with a lock. She wanted to see what was behind it.
"Mommy, can I open this door?" asked Sally. 
"Sure," said Mommy. 
Sally put her key in the lock and turned it.  The door creaked open and Sally stepped inside.
Sally was surprised to find a nice place. She saw a cozy fireplace, big comfy chairs and a fluffy bed. 
Sally was so excited. She sat on the bed and felt the softness beneath her. 
Sally was so happy to have found this place. She knew it was going to be a nice place to relax and be happy.
```

/tmp/out1.txt

```
Sally was having a nice day. She was out in the garden, playing with her ball. Suddenly, a bee came buzzing by.
“Oh no!” exclaimed Sally. “What should I do?”
The bee smiled at Sally and said, “Don’t be scared! I’m just looking for some nectar.”
“What are you looking for?” asked Sally.
“It’s a zip,” said the bee. “I need it to make a delicious honeycomb for my honey.”
Sally watched the bee fly away and she thought about what the bee had said. She realized that even the smallest things can be helpful. 
The moral of this story is that small things can make a big difference.
```

etc

$ clamma-gen-multi llama2_7b_q80.bin -i "lustrous hair "

```
☙ Clamma ❧  8 x pthreads, model: llama2_7b_q80.bin (6828MB) int8 MMAP, vocab: 32000 (423KB),
             d: 4096, hd: 11008, l: 32, h: 32, kvh: 32, seq_len: 2048
  Session: 2048.627MB
    Query: temp: 1.000000, topp: 0.900000, seed: 1701852665388695933
  Session: 2048.627MB
    Query: temp: 1.000000, topp: 0.900000, seed: 1701852665388802764
  Session: 256 tokens, tok/s: 0.819
  Session: 256 tokens, tok/s: 0.819
```

This produced two results concurrently, /tmp/out0.txt

```
____________
Years have passed, more than enough to wash away all manner of evil.  
There is no use in nursing these old grievances.  
You must follow my path, you must make peace with the mistakes of the past.  
Everyone makes mistakes, after all.  
And the more you learn from those mistakes, the better you will be.
Afew days ago I was awakened in the night by a pair of ears pressed to my window.
Together with the soft thud of lustrous hair, the noise of a lamb bouncing off the glass of a deserted building. It was a familiar sound, even with no sight of the creature's body.  
The memory was there, a pressure like a nail pressing on my throat.  
No one must ever know, but I knew the animal belonged to Saul.
Before I knew it, I was lying awake in the dark.  
His thoughts as well, following me through the darkness.
A girl who would know more, if she spoke of it.  
But she has no reason to speak.  
Nothing
```

/tmp/out1.txt

```
lustrous hair In keeping with the title of this book, "lustrous hair" is, for a person, his or her shine. It was out of that oxymoron (one that is both real and false) that I began writing about a certain luster or glow of the human spirit.
While trying to figure out what to do with the new glow in my life, I decided to write about lustrous hair, the kind that has silver highlights or, better yet, long, rich locks of hair, almost like Marilyn Monroe's hair or Anne of Green Gables's hair. Both those women captured a modern ideal of the feminine—yet I really don't know how Marilyn Monroe had that shine. So many people have left their hair in the sun or the salon, many don't comb or wash their hair, let alone use any kind of hair care products. Some women, I know, spend five hours each morning just combing, styling, and curling their hair. When you think about it, not many women wake up at five thirty to have a cup of tea, read a few pages
```


$ clamma-ws-demo

This listens on http://127.0.0.1:7681 and creates two stories simultaneously
on visiting browsers.

## CMake Build options

The default build is single-threaded.  If your target has SMP, you can get
a significant speed increase by building it to use a threading library like
pthreads.

### LIBCLAMMA_WITH_LWS (default: OFF)

Build the `clamma-ws-demo` standalone webserver application, which needs main
branch lws built and installed on the build machine

### LIBCLAMMA_THREADING (default: OFF)

This defaults to OFF, or no SMP acceleration.  If you set it to
`-DLIBCLAMMA_THREADING=PTHREADS`, it will build in support for spreading the
large matrix multiplies over parts that run on different threads / cores
simultaneously.

### LIBCLAMMA_MAX_THREADS (default: 16)

Only active if LIBCLAMMA_THREADING is not OFF.  Maximum amount of threads to
spawn on startup.  The actual count can be set by `info.threads` when creating
the first txf.

### LIBCLAMMA_MAX_THREAD_JOB_QUEUE (default: 256)

Only active if LIBCLAMMA_THREADING is not OFF.  Maximum length of the job ring
buffer used to share matrix multiplication chunks between worker threads.

### LIBCLAMMA_MAX_SESSIONS_PER_MODEL (default: 16)

Max number of simultaneous sessions user code may instantiate on a txf.  This
applies even without threading since single-threaded will round-robin between
the session automatically per-token when doing `clamma_sessions_step_next()`

