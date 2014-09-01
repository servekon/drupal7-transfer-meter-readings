<?php
// $Id: transfer_meter_readings.admin.inc,v 1.5.2.3 2008/11/08 17:35:12 arancaytar Exp $

/**
 * @file transfer_meter_readings.admin.inc
 * Functions that are only called on the admin pages.
 */
$minMoth = '';
$cMon = '';

/*
 * Export meter readings form
 * */
function transfer_meter_readings_export_form($form, &$form_state){
	$form = array();
	
	$form['tmr_only_new_meters'] = array(
	'#type' => 'checkbox',
	'#title' => t('Download only the new meter reading'),
	'#default_value' => 1,
	'#description' => t('If the checkbox is not necessary, it will unload all the counters, these residents.')
	);
	
	$form['submit'] = array(
			'#type' => 'submit',
			'#value' => t('Unload counter readings')
	);
	
	return $form;
}
function transfer_meter_readings_export_form_submit($form, &$form_state) {
	global $user,$arrFileEncode;
	
    $query = db_select('tmr_new_readings', 'nr')
    ->fields('nr');
	
    if((int)$form_state['input']['tmr_only_new_meters'] > 0){
        $query->condition('nr_block', 1);
    }
    else{
        $query->condition('nr_block', 1, '>=');
    }
    
    $result = $query->execute();
    
    $count  = $result->rowCount();
    
	if ($result == false OR empty($count)) {
        drupal_set_message(t("No data."), 'error', FALSE);
        return false;
	}
    
    $blockArr = array(1=>t('New'), 2=>t('Previously unloaded'));
	$fencode = (int)variable_get('tmr_export_file_encode', 1);
    $encode = $arrFileEncode[$fencode];
    
    $fTerm = (int)variable_get('tmr_export_fields_terminated', 0);
    $termArr	= array(
		0 => "\t",
		1 => ";",
		2 => ",",
		3 => "|",
		4 => "^",
	);
    $terminated = $termArr[$fTerm];
    
    $fEncl  = (int)variable_get('tmr_export_fields_enclosed', 0);
    $enclArr	= array(
		0 => "'",
		1 => "\"",
	);
    $enclosed = $enclArr[$fEncl];
    
	$filename 	=	'export_meter_readings_'.date("d.m.Y").'.csv';
	drupal_add_http_header('Content-Type', 'text/csv');
	drupal_add_http_header('Content-Disposition', 'attachment; filename='. $filename);
	drupal_send_headers();
	
    while($row = $result->fetchAssoc()){
        $rows[] = $row;
    }
    
    $opt = array(
        1=>TRUE,
        2=>$encode,
    );
    
    $fp = fopen('php://output', 'w');
    if(variable_get('tmr_export_first_row_is_header', 0) > 0){
        $header = array_keys($rows[0]);
        array_pop($header);
        array_walk($header, '_tmr_translate_col', $opt);
        fputcsv($fp, $header, $terminated, $enclosed);
    }
    $opt[1] = FALSE;
    foreach($rows as $rKey=>$rVal){
        unset($rVal['date_change']);
        $rVal['nr_ins_date'] = format_date($rVal['nr_ins_date'], 'short');
        foreach($rVal as $key=>$val){
          if($key == 'nr_id'){
                $keysArr[] = $val;
          }
          $values[] = decode_entities(strip_tags($val)); 
        }
        array_walk($values, '_tmr_translate_col', $opt);
        fputcsv($fp, $values, $terminated, $enclosed);
        unset($values,$key,$val);
    }
    unset($rKey,$rVal);
    watchdog('rmr-info', 'Export meter readings #'.$filename.' completed. Reception meter readings was <strong>'.$user->name.'</strong> at '.date('d.m.Y H:m:s'));
    fclose($fp);
    
    try{
		db_update('tmr_new_readings')
		->fields(array('nr_block' => 2, 'date_change'=> REQUEST_TIME))
		->condition('nr_id', $keysArr, 'IN')
		->condition('nr_block', 1)
		->execute();
		watchdog('rmr-info', 'Export meters file completed.Reception meter readings was <strong>'.$user->name.'</strong> at '.date('d.m.Y h:m:s'));
	}
	catch(Exception $e){
		watchdog_exception('rmr-error', $e);
		drupal_set_message(t('Error. More information in '.l('syslog','admin/reports/dblog').'.').'<br />'.$e->getMessage(), 'error', FALSE); return false;
	}
	drupal_exit();
}

function _tmr_translate_col(&$item, $key, $opt){
    $fencode = (int)variable_get('tmr_export_file_encode', 1);
    $isHeader = $opt[1];
    $encode = $opt[2];
    
    if($isHeader){
        $item =  preg_replace('/nr_/', '', $item);
        $item =  preg_replace('/_/', ' ', $item);
        $item =  ucfirst($item);
        if(variable_get('tmr_export_translate_columns', 0) > 0){
            $item = t($item);
        }
    }
    
    if($fencode > 1){
        $item = mb_convert_encoding($item, $encode, "UTF-8");
    }
}

function transfer_meter_readings_unblock_readings(){
	$form = array();
	
	$form['alcom_konsalt_yes'] = array(
		'#type' => 'item',
		'#title' => t('Are you sure you want to unlock meter readings?'),
		'#markup' => t('After unlocking will be available to unload all the previously listed indications.'),
		'#weight' => -1,
	);
	
	$form['submit'] = array(
			'#type' => 'submit',
			'#value' => t('Yes'),
			'#weight' => 1,
	);
	
	$form['alcom_konsalt_cancel'] = array(
		'#type' => 'link',
		'#title' => t('Cancel'),
		'#href' => 'admin/transfer-meter-readings/',
		'#weight' => 2,
	);
	return $form;
}

function transfer_meter_readings_unblock_readings_submit(){
	try{
		$result = db_update('tmr_new_readings')
		->fields(array('nr_block' => 1, 'date_change'=> REQUEST_TIME))
		->condition('nr_block', 1, '>')
		->execute();
		
		if ($result == false OR empty($result)){
			drupal_set_message(t("No data."), 'error', FALSE);
		}
		else{
			drupal_set_message(t("Meter readings ublocked."), 'status', FALSE);
		}
		drupal_goto('admin/transfer-meter-readings/export');
	}
	catch(Exception $e){
		watchdog_exception('rmr-error', $e);
		drupal_set_message(t('Error. More information in '.l('syslog','admin/reports/dblog').'.').'<br />'.$e->getMessage(), 'error', FALSE); return false;
	}
}


function transfer_meter_readings_meter_filter_form($form, &$form_state){
	$form = array();
	
	$collapsed 	= true;
	$argType	= (int)arg(4);
	$arg		= check_plain(arg(5));
    
	if(!empty($arg) OR (empty($arg) and $argType==3)){
		$collapsed = false;
	}
	
	$form['tmr_filter'] = array(
      '#type' => 'fieldset',
      '#title' => t('Filter'),
      '#weight' => 5,
      '#collapsible' => TRUE,
      '#collapsed' =>$collapsed,
    );
	
	$form['tmr_filter']['tmr_filter_type'] = array(
		'#type' => 'select',
		'#title' => t('Filter'),
		'#options' => array(
			1 => t('Personal account number'),
			2 => t('Name of meter'),
			3 => t('Only new meter readings'),
		),
		'#default_value' => $argType,
		'#weight' => 1,
	);
	$form['tmr_filter']['tmr_filter_text'] = array(
		'#type' => 'textfield',
		'#value' => $arg,
		'#title' => t('Search'),
		'#weight' => 2,
        '#states' => array(
            'disabled' => array(
                'select[name="tmr_filter_type"]' => array('value' => 3),
            ),
        ),
	);
	$form['tmr_filter']['submit'] = array(
			'#type' => 'submit',
			'#value' => t('Filter'),
			'#weight' => 10,
	);
	$form['tmr_filter']['tmr_cancel'] = array(
		'#type' => 'link',
		'#title' => t('Cancel'),
		'#href' => 'admin/transfer-meter-readings/readings/show-from-user',
		'#weight' => 11,
	);
	return $form;
}

function transfer_meter_readings_meter_filter_form_submit($form, &$form_state){
	$txt 	= (isset($form_state['input']['tmr_filter_text']) ? $form_state['input']['tmr_filter_text'] : '');
	$type	= (int)$form_state['input']['tmr_filter_type'];
    drupal_goto('admin/transfer-meter-readings/readings/show-from-user/'. $type.'/'.$txt);
}

function transfer_meter_readings_meter_reading_template(){
    $filterForm = drupal_get_form('transfer_meter_readings_meter_filter_form');
    $output = drupal_render($filterForm);
    
    $type	= (int)arg(4);
    $text	= check_plain(arg(5));
    
    $meters = array();
    
    $header = array(
        array('data' => '#'),
        array('data' => t('Personal account number'), 'field' => 'fls', 'sort' => 'asc'),
        array('data' => t('Home address'), 'field' => 'st_name'),
        array('data' => t('Name'), 'field' => 'fio'),
        array('data' => t('Meter number'), 'field' => 'met_num'),
        array('data' => t('Counter Name'), 'field' => 'met_name'),
        array('data' => t('Counter type'), 'field' => 'met_type'),
        array('data' => t('Previous reading'), 'field' => 'oldval'),
        array('data' => t('New indications'), 'field' => 'met_newvalue'),
        array('data' => ''),
    );
    
    $query = db_select('tmr_new_readings', 'nr')->extend('PagerDefault')	//Pager Extender
    ->limit(50)
    ->extend('TableSort')
    ->orderByHeader($header);
    
    
    $query->leftJoin('tmr_main_data', 'md', 'nr.nr_account = md.md_account and nr.nr_st_key=md.md_st_key');
            
    if(isset($text) and !empty($text)){
        if($type==1){
            $query->condition('nr.nr_account', '%'.check_plain($text).'%', 'LIKE');
        }
        if($type==2){
            $query->condition('nr.nr_met_name', '%'.check_plain($text).'%', 'LIKE');
        }
    }
    
    if($type==3){
        $query->condition('nr.nr_block', 1);
    }
    
    $query->fields('nr');
    
    $query->addField('md', 'md_name', 'name');
    $query->addField('md', 'md_st_name', 'st_name');
    $query->addField('md','md_house_num', 'house_num');
    $query->groupBy('nr.nr_met_num');
    
    $result = $query
    ->execute();
    
    $count  = $result->rowCount();

    foreach($result as $tm){
        $meters[] = $tm;
    }
    
    $readings['meters'] = $meters;
    $readings['header'] = $header;
    $output .= theme('meters_meter_reading_template', $readings);
    return $output;
}

function transfer_meter_readings_unblock_single_form($form, &$form_state, $id){
	$form = array();
    
    $form['type'] = array(
        '#type' => 'radios',
        '#title' => t('Unblock'),
        '#required' => TRUE,
        '#options' => array(t('For user'), t('For re-export')),
        '#default_value' => 1,
    );
    $form['submit'] = array(
        '#type' => 'submit',
        '#value' => t('Unblock'),
        '#ajax' => array(
			'path' => 'admin/transfer-meter-readings/unblock-single-meter/'.$id,
			'progress' => array('message'=>'.','type'=>'throbber'),
		),
	);
	return $form;
}

/*
 * Ajax callback for get list of houses by street key
 * */
function type_unblock_callback($form, $form_state){
	return $form['submit'];
}

function transfer_meter_readings_blocking_single_form($form, &$form_state, $id){
	$form = array();
	$form['submit'] = array(
        '#type' => 'submit',
        '#value' => t('Block up'),
		'#ajax' => array(
			'path' => 'admin/transfer-meter-readings/blocking-single-meter/'.$id,
			'progress' => array('message'=>'...','type'=>'throbber'),
		),
	);
	return $form;
}

function transfer_meter_readings_unblock_single($id){
    $type = (int)$_POST['type'];
    if($type > 1 OR $type < 0){
        $type = 1;
    }
    try{
        db_update('tmr_new_readings')
        ->fields(array(
                'nr_block' => $type,
                'date_change'=> REQUEST_TIME,
            )
        )
        ->condition('nr_id', $id)
        ->execute();
    }
    catch(Exception $e){
        watchdog_exception('rmr-error', $e);
        drupal_set_message(t('Error. More information in '.l('syslog','admin/reports/dblog').'.').'<br />'.$e->getMessage(), 'error', FALSE); return false;
    }
    
    $form = drupal_get_form('transfer_meter_readings_blocking_single_form', $id);
    $output = drupal_render($form);
    $commands 	= array();
    $colors = array('#006600', '#336699');
    
    $commands[] = ajax_command_html('div#single-meters-meter-'.$id, $output);
    $commands[] = ajax_command_css('span#meters-meter-user-newval-'.$id, array('background-color' => $colors[$type]));
    
    $page = array(
      '#type' => 'ajax',
      '#commands' => $commands,
    );
    ajax_deliver($page);
}

function transfer_meter_readings_blocking_single($id){
    try{
        db_update('tmr_new_readings')
        ->fields(array(
                'nr_block' => 2,
                'date_change'=> REQUEST_TIME,
            )
        )
        ->condition('nr_id', $id)
        ->execute();
    
    }
    catch(Exception $e){
        watchdog_exception('rmr-error', $e);
        drupal_set_message(t('Error. More information in '.l('syslog','admin/reports/dblog').'.').'<br />'.$e->getMessage(), 'error', FALSE); return false;
    }
    $form = drupal_get_form('transfer_meter_readings_unblock_single_form', $id);
    $output = drupal_render($form);
    $commands 	= array();
    
    $commands[] = ajax_command_html('div#single-meters-meter-'.$id, $output);
    $commands[] = ajax_command_css('span#meters-meter-user-newval-'.$id, array('background-color' => '#990000'));
    $page = array(
      '#type' => 'ajax',
      '#commands' => $commands,
    );
    ajax_deliver($page);
}

/**
 * Import data form
 * */
function transfer_meter_readings_import_form($form=array(), &$form_state){
	
	$count  = 0;
	
	$form['tmr_import_file'] = array(
		'#type' => 'file', 
		'#title' => t('Choose a file'), 
		'#size' => 40,
		'#weight' => 1,
	);
	
    $form['tmr_import_rebuild'] = array(
		'#type' => 'checkbox',
		'#title' => t('Re-create a temporary table'),
		'#return_value' => 1,
		'#default_value' => 1,
		'#weight' => 2,
    );
    
	$form['submit'] = array(
			'#type' => 'submit',
			'#value' => t('Save and continue'),
			'#weight' => 10,
	);
	
	$form['tmr_cancel'] = array(
		'#type' => 'link',
		'#title' => t('Cancel'),
		'#href' => 'admin/transfer-meter-readings/import',
		'#weight' => 11,
	);
	
	return $form;
}

function transfer_meter_readings_import_form_submit($form, &$form_state){
	module_load_install('transfer_meter_readings');
	$usFieldStuct = transfer_meter_readings_schema();
	$usFieldStuct = $usFieldStuct['tmr_main_data'];
	
	$tblBulk 	= 'tmr_import_tmp';
	$file 		= $_FILES['files']['tmp_name']['tmr_import_file'];
	$tblUser = $arrField = $insArrField = $notNullArr = array();
	$recreate	= (int)$form_state['values']['tmr_import_rebuild'];
	$termArr	= array(
		0 => '\\t',
		1 => ';',
		2 => ',',
		3 => '|',
		4 => '^',
	);

	$enclArr	= array(
		0 => '\\\'',
		1 => '"',
	);

	$lineTerArr	= array(
		0 => '\\r\\n',
		1 => '\\n',
		2 => '\\r',
	);
	
	$escdArr 	= array('\\\\');
	
	$schema['fields']['id'] = array('type' => 'serial', 'size' => 'big', 'not null' => TRUE, 'description'=> "Fields ID");
	$query = db_select('tmr_fields_table', 'f')
    ->fields('f', array('f_user_field', 'f_id'))
    ->orderBy('f_weight', 'ASC')
    ->execute()
    ->fetchAllAssoc('f_id');
	
	foreach($query as $field){
		$Fields[$field->f_user_field] = $field->f_user_field;
		$arrField[] = '`'.$field->f_user_field.'`';
		$insArrField[] = $field->f_user_field;
        $notNullArr[$field->f_user_field] = $usFieldStuct['fields'][$field->f_user_field]['not null'];
        if(isset($usFieldStuct['indexes'][$field->f_user_field])){
            $schema['indexes'][$field->f_user_field] = $usFieldStuct['indexes'][$field->f_user_field];
        }
        $schema['fields'][$field->f_user_field] = array('type' => 'varchar', 'size' => 'normal', 'not null' => TRUE, 'length' => 255);
	}
	
	$schema['primary key'] = array('id');
	
	//Create table from fields config
	if(!db_table_exists($tblBulk) or ($recreate >0 and db_table_exists($tblBulk))){
		try{
			db_drop_table($tblBulk);
			db_create_table($tblBulk, $schema);
		}
		catch(Exception $e){
			watchdog_exception('rmr-error', $e);
			drupal_set_message(t('When you create a temporary table, an error occurred.'), 'warning'); return false;
		}
	}
	elseif(!empty($file)){
		db_truncate($tblBulk)->execute();
	}
	//Import data from CSV file
	if(!empty($file)){
		try{
			$terminated = (int)variable_get('tmr_fields_terminated', 0);
			$enclosed	= (int)variable_get('tmr_fields_enclosed', 0);
			$escaped	= (int)variable_get('tmr_fields_escaped', 0);
			$line_term	= (int)variable_get('tmr_lines_terminated', 0);
			
			$q = 'LOAD DATA LOCAL INFILE :file INTO TABLE {'.check_plain($tblBulk).'} CHARACTER SET utf8 FIELDS TERMINATED BY  \''.$termArr[$terminated].'\' ENCLOSED BY  \''.$enclArr[$enclosed].'\' ESCAPED BY  \''.$escdArr[$escaped].'\' LINES TERMINATED BY  \''.$lineTerArr[$line_term].'\' ('.implode(',', $arrField).')';
			db_query($q, array(':file'=>$file), array(PDO::MYSQL_ATTR_LOCAL_INFILE => 1));
            
            //~ Remove empty rows for NOT NULL fields
            foreach($notNullArr as $nkey=>$nval){
                if($nval){
                    db_delete($tblBulk)->condition($nkey, '')->execute();
                }
            }
            //~ Truncate table
            db_truncate('tmr_main_data')->execute();
            //~ Fill data
            $qToIns = db_select($tblBulk, 's')->fields('s', $insArrField);
            $qToIns->addExpression(REQUEST_TIME, 'date_change'); 
            
            $insArrField[] = 'date_change';
            db_insert('tmr_main_data')->fields($insArrField)->from($qToIns)->execute();
            
             //~ Need for first step form
            _tmr_fill_handbook_housing_stock();
            
            db_query('UPDATE {tmr_main_data} SET `md_flat` = TRIM(`md_flat`)');
            
            drupal_set_message(t('Import complete.'));
		}
		catch(Exception $e){
			watchdog_exception('rmr-error', $e);
			drupal_set_message('When importing file, an error occurred. More information in '.l('syslog','admin/reports/dblog').'.'.'<br />'.$e->getMessage(), 'error', FALSE); return false;
		}
	}
}

//~ Fill "tmr_handbook_housing_stock" table
function _tmr_fill_handbook_housing_stock(){
	try{
		db_truncate('tmr_handbook_housing_stock')->execute();
		
        $qToIns = db_select('tmr_main_data', 'md')->fields('md', array('md_st_key', 'md_st_name', 'md_house_key', 'md_house_num'));
        $qToIns->addExpression(REQUEST_TIME, 'date_change');
        $qToIns->groupBy('md_st_key,md_st_name,md_house_key,md_house_num');
        
        db_insert('tmr_handbook_housing_stock')->fields(array('hhs_st_key', 'hhs_st_name', 'hhs_house_key', 'hhs_hs_house', 'date_change'))
        ->from($qToIns)->execute();
        
        drupal_set_message(t('Filling handbook addresses successful.'));
	}
	catch(Exception $e){
		watchdog_exception('rmr-error', $e);
		drupal_set_message(t('When filling in the address directory, an error occurred. More information in '.l('syslog','admin/reports/dblog').'.').'<br />'.$e->getMessage(), 'error', FALSE); return false;
	}
	return true;
}

/**
 * Manage fields form
 * */
function tmr_conf_manage_fields_form($form, &$form_state){
	$form['tmr_fields']['#tree'] = true;
	
	$result = db_select('tmr_fields_table', 'f')
    ->fields('f')
    ->orderBy('f_weight', 'ASC')
    ->execute()
    ->fetchAllAssoc('f_id');
    
    foreach($result as $field){
		$form['tmr_fields'][$field->f_id]=array(
			'user_field' => array(
				'#markup' => check_plain($field->f_user_field)
			),
			'title' => array(
				'#type' => 'textfield',
				'#default_value' => check_plain($field->f_title),
				'#size' => 20,
				'#maxlength' => 255,
			),
			'actions' =>array(
				'#markup' => l(t('Delete'), 'admin/transfer-meter-readings/settings/fields/del/'.$field->f_id)
			),
			'weight' => array(
				'#type' => 'weight',
				'#title' => t('Weight'),
				'#default_value' => $field->f_weight,
				'#delta' => 10,
				'#title-display' => 'invisible',
			),
		);
	}
	
	$form['actions'] = array('#type' => 'actions');
	$form['actions']['submit'] = array('#type' => 'submit', '#value' => t('Save settings'));
	return $form;
}


function theme_tmr_conf_manage_fields_form($variables) {
  $form = $variables['form'];

  // Initialize the variable which will store our table rows.
  $rows = array();

  // Iterate over each element in our $form['example_items'] array.
  foreach (element_children($form['tmr_fields']) as $id) {

    
    $form['tmr_fields'][$id]['weight']['#attributes']['class'] = array('rmr-field-weight');

    $rows[] = array(
      'data' => array(
        // Add our 'user_field' column.
        drupal_render($form['tmr_fields'][$id]['user_field']),
        // Add our 'title' column.
        drupal_render($form['tmr_fields'][$id]['title']),
        drupal_render($form['tmr_fields'][$id]['actions']),
        // Add our 'weight' column.
        drupal_render($form['tmr_fields'][$id]['weight']),
      ),
      'class' => array('draggable'),
    );
  }

  $header = array(t('Field of main data table'), t('Title'),  t('Actions'), t('Weight'));

  $table_id = 'rmr-fields-table';

  // We can render our tabledrag table for output.
  $output = theme('table', array(
    'header' => $header,
    'rows' => $rows,
    'attributes' => array('id' => $table_id),
    'empty' => t('No fields.'),
  ));

  // And then render any remaining form elements (such as our submit button).
  $output .= drupal_render_children($form);

  drupal_add_tabledrag($table_id, 'order', 'sibling', 'rmr-field-weight');

  return $output;
}

/**
 * Manage fields form submit
 * */
function tmr_conf_manage_fields_form_submit($form, &$form_state){
	foreach ($form_state['values']['tmr_fields'] as $id => $field) {
		db_update('tmr_fields_table')
		->fields(array(
				'f_title' => check_plain($field['title']),
				'f_weight'=> $field['weight'],
				'date_change'=> REQUEST_TIME,
		))
		->condition('f_id', (int)$id)
		->execute();
	}
}


/**
 * Form for add new field
 * */
function tmr_conf_add_field_form($form=array(), &$form_state){
	
	$fields = $noFields = $weightArr = $titles = $reqArr = array();
	$suff = '';
    
	for($i=1; $i<15; $i++){
		$weightArr[$i] = $i;
	}
	 	
	try{
		$getFields 	= db_query('SHOW FULL COLUMNS FROM {tmr_main_data}');
		$extFields	= db_query('SELECT `f_user_field` as field, `f_weight` as weight FROM {tmr_fields_table}');
		$maxWeight	= db_query('SELECT MAX(`f_weight`) as m FROM {tmr_fields_table}')->fetchObject();
		
        foreach ($getFields as $field){
            if($field->Field != 'md_id' and $field->Field != 'date_change'){
                $fields[$field->Field] = $field->Field;
                $reqArr[$field->Field] = $field->Null;
                $titles[$field->Field] = $field->Comment;
            }
		}
		foreach($extFields as $extField){
			$noFields[$extField->field] = $extField->field;
            unset($weightArr[$extField->weight]);
		}
		//Allow add only previously unmounted fields
		$diffFields	= array_diff($fields, $noFields);
	}
	catch(Exception $e){
		watchdog_exception('rmr-error', $e);
		drupal_set_message(t('Error'), 'warning'); return false;
	}
    
    if(empty($diffFields)){
        drupal_set_message(t('No available fields.'), 'warning');
        drupal_goto('admin/transfer-meter-readings/settings/fields');
    }
    
    //~ Set friendly titles
    foreach($diffFields as $key=>$val){
        //~ Field NULL or NOT NULL
        $suff = ' (require)';
        if(strlen($reqArr[$key]) > 2){
            $suff = ' (no require)';
        }
        $diffFields[$key] = $titles[$key].$suff;
    }
    
    $form['user_field']=array(
		'#type' => 'select',
		'#title' => t('Field of main data table'),
		'#required' => TRUE,
		'#options' => $diffFields,
	);

	$form['title']=array(
		'#type' => 'textfield',
		'#title' => t('Title'),
		'#required' => TRUE,
		'#size' => 50,
	);
	
	$form['weight']=array(
		'#type' => 'select',
		'#title' => t('Weight'),
		'#required' => TRUE,
		'#default_value' => ($maxWeight->m+1),
		'#options' => $weightArr,
	);
	
	$form['submit']=array(
		'#type' => 'submit',
		'#value' => t('Add field'),
	);
	$form['tmr_cancel'] = array(
		'#type' => 'link',
		'#title' => t('Cancel'),
		'#href' => 'admin/transfer-meter-readings/settings/fields',
	);
	return $form;
}

function tmr_conf_add_field_form_submit($form, &$form_state){
	form_state_values_clean($form_state);
	
	$q 		= $form_state['values'];
	$isData = 0;
	$field	= check_plain($q['user_field']);
	
	try{
		$ins = db_insert('tmr_fields_table')->fields(array(
			'f_title' => $q['title'],
			'f_user_field' => $field,
			'f_weight' => (int)$q['weight'],
            'date_change'=> REQUEST_TIME,
		))->execute();
		drupal_set_message(t('Field added.'), 'status'); return true;
	}
	catch(Exception $e){
		watchdog_exception('rmr-error', $e);
		drupal_set_message(t('Error'), 'warning'); return false;
	}
}

/**
 * Delete field
 * */
function tmr_conf_del_field($id){
	$form = drupal_get_form('tmr_conf_del_field_form', $id);
    return drupal_render($form);
}

/**
 * Delete field form
 * */
function tmr_conf_del_field_form($form=array(), &$form_state, $id){
	
	$field = db_query('SELECT `f_title` FROM {tmr_fields_table} WHERE f_id = :id LIMIT 0,1', array(':id'=>$id))->fetchObject();
    if($field){
        $form['del_id']=array(
            '#type' => 'hidden',
            '#value' => (int)$id,
        );
        
        $form['confirm']=array(
            '#type' => 'item',
            '#markup' => t('Do you want to remove a field "%field"?', array('%field'=>(isset($field->f_title) ? $field->f_title : ''))),
        );
        
        $form['submit'] = array(
                '#type' => 'submit',
                '#value' => t('Yes'),
        );
        
        $form['tmr_cancel'] = array(
            '#type' => 'link',
            '#title' => t('Cancel'),
            '#href' => 'admin/transfer-meter-readings/settings/fields',
        );
    }
    else{
        $form['error'] = array(
            '#type' => 'item',
            '#markup'=>t('Field not found.'),
        );
        
        $form['tmr_cancel'] = array(
            '#type' => 'link',
            '#title' => t('Cancel'),
            '#href' => 'admin/transfer-meter-readings/settings/fields',
        );
    }
	return $form;
}

/**
 * Delete field form submit
 * */
function tmr_conf_del_field_form_submit($form, &$form_state){
	$id = $form_state['values']['del_id'];
	try{
		db_delete('tmr_fields_table')->condition('f_id', $id)->execute();
		drupal_set_message(t('Field deleted.'));
	}
	catch(Exception $e){
		watchdog_exception('rmr-error', $e);
		drupal_set_message(t('Error'), 'warning'); return false;
	}
	drupal_goto('admin/transfer-meter-readings/settings/fields');
}

/**
 * Seettings of files format
 * */
function tmr_settings_form($form, &$form_state){
	global $arrFileEncode;
    $form['group_settings'] = array(
		'#type' => 'vertical_tabs',
	);
    
    $form['tmr_main_fieldset'] = array(
        '#type' => 'fieldset',
        '#title' => t('Input settings'),
        '#weight' => -1,
        '#collapsible' => TRUE,
        '#collapsed' => TRUE,
        '#group' => 'group_settings',
    );
	
    $form['tmr_main_fieldset']['tmr_delta_meter_readings'] = array(
		'#type' => 'textfield',
		'#title' => t('The maximum possible difference between the previous and the current counter value'),
		'#description' => t('Set "0" to not use validation volume difference between the current and previous meter readings'), 
		'#return_value' => 1, 
		'#default_value' => (int)variable_get('tmr_delta_meter_readings', 0),
    );
	
	$form['tmr_main_fieldset']['tmr_is_ceil_meter_readings'] = array(
		'#type' => 'checkbox',
		'#title' => t('Round meter readings to the nearest whole number'),
		'#return_value' => 1, 
		'#default_value' => (int)variable_get('tmr_is_ceil_meter_readings', 0),
    );
    
    $form['tmr_import_fieldset'] = array(
        '#type' => 'fieldset',
        '#title' => t('Import settings'),
        '#weight' => 3,
        '#collapsible' => TRUE,
        '#collapsed' => TRUE,
        '#group' => 'group_settings',
    );
    
    $form['tmr_import_fieldset']['tmr_fields_terminated']=array(
		'#type' => 'select',
		'#title' => t('The field separator'),
		'#default_value' => (int)variable_get('tmr_fields_terminated', 0),
		'#size' => 1,
		'#options' => array(
			0 => 'TAB',
			1 => ';',
			2 => ',',
			3 => '|',
			4 => '^',
		),
		'#required' => TRUE,
	);
	//~ ESCAPED
	$form['tmr_import_fieldset']['tmr_fields_enclosed']=array(
		'#type' => 'select',
		'#title' => t('The fields are enclosed in'),
		'#default_value' => (int)variable_get('tmr_fields_enclosed', 0),
		'#size' => 1,
		'#options' => array(
			0 => '\' ('.t('Single quote').')',
			1 => '" ('.t('Double quote').')',
		),
		'#required' => TRUE,
	);
	
	$form['tmr_import_fieldset']['tmr_fields_escaped']=array(
		'#type' => 'select',
		'#title' => t('The escape character'),
		'#default_value' => (int)variable_get('tmr_fields_escaped', 0),
		'#size' => 1,
		'#options' => array(
			0 => '\\',
		),
		'#required' => TRUE,
	);
	
	$form['tmr_import_fieldset']['tmr_lines_terminated']=array(
		'#type' => 'select',
		'#title' => t('Line separator'),
		'#default_value' => (int)variable_get('tmr_lines_terminated', 0),
		'#size' => 1,
		'#options' => array(
			0 => '\\r\\n',
			1 => '\\n',
			2 => '\\r',
		),
		'#required' => TRUE,
	);
	
    $form['tmr_export_fieldset'] = array(
      '#type' => 'fieldset',
      '#title' => t('Export settings'),
      '#weight' => 1,
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
      '#group' => 'group_settings',
    );
    
    $form['tmr_export_fieldset']['tmr_export_first_row_is_header'] = array(
		'#type' => 'checkbox',
		'#title' => t('Add in the first row the name of columns'),
        '#weight' => -2,
		'#default_value' => variable_get('tmr_export_first_row_is_header', 0),
    );
    
    $form['tmr_export_fieldset']['tmr_export_translate_columns'] = array(
		'#type' => 'checkbox',
		'#title' => t('Translate column names'),
        '#weight' => -1,
		'#default_value' => variable_get('tmr_export_translate_columns', 0),
        '#states' => array(
            'visible' => array(
                ':input[name="tmr_export_first_row_is_header"]' => array('checked' => TRUE),
            ),
            'disabled' => array(
                ':input[name="tmr_export_first_row_is_header"]' => array('checked' => FALSE),
            ),
        ),
    );
    
    $form['tmr_export_fieldset']['tmr_export_file_encode'] = array(
        '#type' => 'select',
        '#title' => t('File encoding'),
        '#required' => TRUE,
        '#default_value' => variable_get('tmr_export_file_encode', 1),
        '#weight' => 3,
        '#options' => $arrFileEncode,
    );
    $form['tmr_export_fieldset']['tmr_export_fields_terminated'] = $form['tmr_import_fieldset']['tmr_fields_terminated'];
	$form['tmr_export_fieldset']['tmr_export_fields_terminated']['#default_value'] = (int)variable_get('tmr_export_fields_terminated', 0);
    
    $form['tmr_export_fieldset']['tmr_export_fields_enclosed'] = $form['tmr_import_fieldset']['tmr_fields_enclosed'];
	$form['tmr_export_fieldset']['tmr_export_fields_enclosed']['#default_value'] = (int)variable_get('tmr_export_fields_enclosed', 0);
    
    return system_settings_form($form);
}
