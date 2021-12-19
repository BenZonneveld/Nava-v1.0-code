//-------------------------------------------------
//                  NAVA v1.x
//                 SEQ Functions
//-------------------------------------------------

/////////////////////Function//////////////////////
//Initialyse the sequencer before to run
void InitSeq()
{
  LoadSeqSetup();
  ppqn = 0;
  stepCount = 0;
  seq.configPage = 0;
  seq.configMode = FALSE;
  randomSeed(analogRead(0));
  seq.dir = FORWARD;
  curSeqMode = seq.BootMode;
  seq.bpm = seq.defaultBpm;
  SetSeqSync();                               // [zabox] [1.028] moved
  seq.syncChanged = FALSE;
}

//Combine OH and CH pattern to trig HH and set total accent for ride and crash
void SetHHPattern()
{
  pattern[ptrnBuffer].inst[HH] = pattern[ptrnBuffer].inst[CH] | pattern[ptrnBuffer].inst[OH];
  if (curSeqMode != PTRN_TAP){
    for (int a = 0; a < NBR_STEP; a++){
      if (bitRead(pattern[ptrnBuffer].inst[CH],a) && curInst == CH) bitClear(pattern[ptrnBuffer].inst[OH],a);
      if (bitRead(pattern[ptrnBuffer].inst[OH],a) && curInst == OH){
        bitClear(pattern[ptrnBuffer].inst[CH],a);
        pattern[ptrnBuffer].velocity[CH][a] = instVelHigh[HH];
      }      
    }
  }
}

//copy pattern to buffer
void CopyPatternToBuffer(byte patternNum)
{
  for (byte i = 0; i < NBR_INST; i++){ 
    bufferedPattern.inst[i] =  pattern[ptrnBuffer].inst[i] ;
  }
  bufferedPattern.length = pattern[ptrnBuffer].length;
  bufferedPattern.scale = pattern[ptrnBuffer].scale;
  bufferedPattern.shuffle = pattern[ptrnBuffer].shuffle;
  bufferedPattern.flam = pattern[ptrnBuffer].flam;
  bufferedPattern.extLength = pattern[ptrnBuffer].extLength;
  bufferedPattern.totalAcc = pattern[ptrnBuffer].totalAcc;

  for ( byte j = 0; j < pattern[ptrnBuffer].extLength; j++){
    bufferedPattern.extNote[j] = pattern[ptrnBuffer].extNote[j];
  }
  for (byte i = 0; i < NBR_INST; i++){//loop as many instrument for a page
    for (byte j = 0; j < NBR_STEP; j++){
      bufferedPattern.velocity[i][j] = pattern[ptrnBuffer].velocity[i][j]; 
    }
  }
}

//paste buffer to current pattern
void PasteBufferToPattern(byte patternNum)
{
  for (byte i = 0; i < NBR_INST; i++){ 
    pattern[ptrnBuffer].inst[i] =  bufferedPattern.inst[i];
  }
  pattern[ptrnBuffer].length = bufferedPattern.length;
  pattern[ptrnBuffer].scale = bufferedPattern.scale;
  pattern[ptrnBuffer].shuffle = bufferedPattern.shuffle;
  pattern[ptrnBuffer].flam = bufferedPattern.flam;
  pattern[ptrnBuffer].extLength = bufferedPattern.extLength;
  pattern[ptrnBuffer].totalAcc = bufferedPattern.totalAcc;

  for ( byte j = 0; j < bufferedPattern.extLength; j++){
    pattern[ptrnBuffer].extNote[j] = bufferedPattern.extNote[j];
  }
  for (byte i = 0; i < NBR_INST; i++){//loop as many instrument for a page
    for (byte j = 0; j < NBR_STEP; j++){
      pattern[ptrnBuffer].velocity[i][j] = bufferedPattern.velocity[i][j]; 
    }
  }

}

//init pattern
void InitPattern()
{
  if (!group.priority && ! group.isLoaded ){
    group.length = pattern[ptrnBuffer].groupLength;
    group.firstPattern = curPattern - pattern[ptrnBuffer].groupPos;
  }
  //Init Ride, Crash velocity to HIGH_VEL
  for (int stp = 0; stp < NBR_STEP; stp++){
    if (pattern[ptrnBuffer].velocity[CH][stp] == 0)  pattern[ptrnBuffer].velocity[CH][stp] = instVelHigh[HH];//HH
    pattern[ptrnBuffer].velocity[CRASH][stp] = instVelHigh[CRASH];//CRASH
    pattern[ptrnBuffer].velocity[RIDE][stp] = instVelHigh[RIDE];//RIDE
    pattern[ptrnBuffer].velocity[TOTAL_ACC][stp] = HIGH_VEL;//TOTAL_ACC
    pattern[ptrnBuffer].velocity[TRIG_OUT][stp] = HIGH_VEL;//TRIG_OUT
//    pattern[ptrnBuffer].velocity[EXT_INST][stp] = HIGH_VEL;//EXT_INST
  }
  if ( group.length )
  {
    prevShuf = pattern[ptrnBuffer].shuffle;
    prevFlam = pattern[ptrnBuffer].flam;
  }
  switch (pattern[ptrnBuffer].scale){
  case  SCALE_16:
    scaleBtn.counter = 0;
    break;
  case  SCALE_32:
    scaleBtn.counter = 1;
    break;
  case  SCALE_8t:
    scaleBtn.counter = 2;
    break;
  case  SCALE_16t:
    scaleBtn.counter = 3;
    break;
  }
}

//Convert Instrument Word to Step Word
void InstToStepWord()
{
  for (int a = 0; a < NBR_STEP; a++){
    pattern[ptrnBuffer].step[a] = 0;
    for (int b = 0; b < NBR_INST; b++){
      if (bitRead(pattern[ptrnBuffer].inst[b],a)) bitSet(pattern[ptrnBuffer].step[a],b);
    }
  }
}

//shift left pattern
void ShiftLeftPattern()
{
  patternWasEdited = TRUE;
  if (instBtn){//only shift selected instruments
    if (bitRead(pattern[ptrnBuffer].inst[curInst],0)){
      pattern[ptrnBuffer].inst[curInst] = pattern[ptrnBuffer].inst[curInst]>>1 | (1<<15);
      if (pattern[ptrnBuffer].velocity[curInst][0] > instVelLow[curInst]) pattern[ptrnBuffer].velocity[curInst][15] = instVelHigh[curInst];
      else pattern[ptrnBuffer].velocity[curInst][15] = instVelLow[curInst];
    }
    else pattern[ptrnBuffer].inst[curInst] = pattern[ptrnBuffer].inst[curInst]>>1;
    //update instrument velocity
    for (int stp = 0; stp < NBR_STEP - 1; stp++){
      pattern[ptrnBuffer].velocity[curInst][stp] = pattern[ptrnBuffer].velocity[curInst][stp+1];
    }
  }
  else{// shift entire pattern
    for (int nbrInst = 0; nbrInst < NBR_INST; nbrInst++){
      if (bitRead(pattern[ptrnBuffer].inst[nbrInst],0)){
        pattern[ptrnBuffer].inst[nbrInst] = pattern[ptrnBuffer].inst[nbrInst]>>1 | (1<<15);
        if (pattern[ptrnBuffer].velocity[nbrInst][0] > instVelLow[nbrInst]) pattern[ptrnBuffer].velocity[nbrInst][15] = instVelHigh[nbrInst];
        else pattern[ptrnBuffer].velocity[nbrInst][15] = instVelLow[nbrInst];
      }
      else pattern[ptrnBuffer].inst[nbrInst] = pattern[ptrnBuffer].inst[nbrInst]>>1;
      //update instrument velocity
      for (int stp = 0; stp < NBR_STEP - 1; stp++){
        pattern[ptrnBuffer].velocity[nbrInst][stp] = pattern[ptrnBuffer].velocity[nbrInst][stp+1];
      }
    }
  }
}

//Shift Right pattern
void ShiftRightPattern()
{
  patternWasEdited = TRUE; 
  if (instBtn){//shift only selected instrument
    if (bitRead(pattern[ptrnBuffer].inst[curInst],15)){
      pattern[ptrnBuffer].inst[curInst] = pattern[ptrnBuffer].inst[curInst]<<1 | 1;
      if (pattern[ptrnBuffer].velocity[curInst][15] > instVelLow[curInst]) pattern[ptrnBuffer].velocity[curInst][0] = instVelHigh[curInst];
      else pattern[ptrnBuffer].velocity[curInst][0] = instVelLow[curInst];
    }
    else pattern[ptrnBuffer].inst[curInst] = pattern[ptrnBuffer].inst[curInst]<<1;
    //update instrument velocity
    for (int stp = NBR_STEP - 1; stp > 0; stp--){
      pattern[ptrnBuffer].velocity[curInst][stp] = pattern[ptrnBuffer].velocity[curInst][stp-1];
    }
  }
  else{//shift entire pattern
    for (int nbrInst = 0; nbrInst < NBR_INST; nbrInst++){
      if (bitRead(pattern[ptrnBuffer].inst[nbrInst],15)){
        pattern[ptrnBuffer].inst[nbrInst] = pattern[ptrnBuffer].inst[nbrInst]<<1 | 1;
        if (pattern[ptrnBuffer].velocity[nbrInst][15] > instVelLow[nbrInst]) pattern[ptrnBuffer].velocity[nbrInst][0] = instVelHigh[nbrInst];
        else pattern[ptrnBuffer].velocity[nbrInst][0] = instVelLow[nbrInst];
      }
      else pattern[ptrnBuffer].inst[nbrInst] = pattern[ptrnBuffer].inst[nbrInst]<<1;
      //update instrument velocity
      for (int stp = NBR_STEP - 1; stp > 0; stp--){
        pattern[ptrnBuffer].velocity[nbrInst][stp] = pattern[ptrnBuffer].velocity[nbrInst][stp-1];
      }
    }
  }
}

void PatternLoad()
{
  byte groupPos = 0;
  
  if ( !group.isLoaded || nextPattern < group.firstPattern || nextPattern > (group.firstPattern + group.length) )
  {        
    LoadPattern(nextPattern);
    // If not a member of a pattern group clear bitmasks
    if (nextPattern < group.firstPattern || nextPattern > (group.firstPattern + group.length) )
    {
      patternNeedSaved = FALSE;
      groupNeedSaved = FALSE;
      group.isLoaded = FALSE; // Unload group
      group.length = 0; 
      groupPatternLoaded = 0;
      groupPatternEdited = 0;
    }
    if ( group.priority ) {
      groupPos = group.pos; 
    } else {
      groupPos = pattern[!ptrnBuffer].groupPos;
    }
  }
  
  if ( group.length > 0 )
  {
    if ( !group.isLoaded)
    {
      byte firstPattern;

      if ( group.priority )
      {
        firstPattern = nextPattern - group.pos;
        groupPos = group.pos;
      } else {
        firstPattern = nextPattern - pattern[!ptrnBuffer].groupPos;
        groupPos = pattern[!ptrnBuffer].groupPos;
      }
      if ( !bitRead(groupPatternLoaded, groupPos) )
      {
        memcpy(&patternGroup[groupPos],&pattern[!ptrnBuffer], sizeof(Pattern));
        bitSet(groupPatternLoaded,groupPos);
      }
      byte bitcount = 0;
      for ( int i=0; i <= group.length; i++ )
      {
        bitcount += ((groupPatternLoaded >> i) &0x1);
      }
      if ( (bitcount - 1) == group.length )
      {
        group.isLoaded = TRUE;
        group.priority = FALSE;
      }
    } else {
      groupPos = nextPattern - group.firstPattern;
    }
    memcpy(&pattern[!ptrnBuffer], &patternGroup[groupPos],sizeof(Pattern));
  } 
  else
  {
    group.isLoaded = FALSE;
    group.priority = FALSE;
    group.length = 0;
    groupPatternLoaded = 0; // Reset bitmask
    groupPatternEdited = 0; // Reset bitmask
  }
}
