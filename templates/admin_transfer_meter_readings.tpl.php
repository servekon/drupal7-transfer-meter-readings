<?php
$rows=array();
$i=1;
foreach($meters as $key=>$val){
	$form	= ($val->nr_block > 1) ? drupal_get_form('transfer_meter_readings_unblock_single_form', $val->nr_id) : drupal_get_form('transfer_meter_readings_blocking_single_form', $val->nr_id);
	switch($val->nr_block){
        case 0:
            $class = 'meter-unblock';
        break;
        case 1:
            $class = 'meter-block-for-user';
        break;
        case 2:
            $class = 'meter-block';
        break;
    }

	$date_change = (!empty(strtotime($val->date_change))) ? strtotime($val->date_change) : 0;
	
	$rows[]=array('data' => array(
            $i,
            l($val->nr_account,'admin/transfer-meter-readings/readings/show-from-user/1/'.$val->nr_account),
            $val->st_name.'<br />д.№ '.trim($val->house_num).', кв.№ '.trim($val->nr_flat),
            $val->name,
            $val->nr_met_num,
            $val->nr_met_name,
            $val->nr_met_type,
            $val->nr_met_val,
            '<span class="'.$class.'" id="meters-meter-user-newval-'.$val->nr_id.'">'.$val->nr_met_new_val.'</span><br />'.format_date($val->nr_ins_date, 'short'),
            '<div id="single-meters-meter-'.$val->nr_id.'">'.drupal_render($form).'</div>',
        )
	);	
	$i++;
}
unset($key,$val,$meters);

$html = theme('table',
    array(
        'header' => $header,
        'rows'=>$rows,
        'empty' => t("No data."),
    )
);

$html .= theme('pager',
    array(
        'tags' => array()
    )
);

print $html;

